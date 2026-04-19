// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include "../../../common/assert.h"
#include "../../../common/hacks/hack_manager.h"
#include "../../../common/logging/log.h"
#include "../../../common/string_util.h"
#include "../../core.h"
#include "../ipc.h"
#include "../kernel/client_port.h"
#include "../kernel/handle_table.h"
#include "../kernel/process.h"
#include "../kernel/server_port.h"
#include "../kernel/server_session.h"
#include "ac/ac.h"
#include "act/act.h"
#include "am/am.h"
#include "apt/apt.h"
#include "boss/boss.h"
#include "cam/cam.h"
#include "cam/y2r_u.h"
#include "cecd/cecd.h"
#include "cfg/cfg.h"
#include "csnd/csnd_snd.h"
#include "dlp/dlp.h"
#include "dsp/dsp_dsp.h"
#include "err/err_f.h"
#include "frd/frd.h"
#include "fs/archive.h"
#include "fs/fs_user.h"
#include "gsp/gsp.h"
#include "gsp/gsp_lcd.h"
#include "hid/hid.h"
#include "http/http_c.h"
#include "ir/ir.h"
#include "ldr_ro/ldr_ro.h"
#include "mcu/mcu.h"
#include "mic/mic_u.h"
#include "mvd/mvd.h"
#include "ndm/ndm_u.h"
#include "news/news.h"
#include "nfc/nfc.h"
#include "nim/nim.h"
#include "nwm/nwm.h"
#include "plgldr/plgldr.h"
#include "pm/pm.h"
#include "ps/ps_ps.h"
#include "ptm/ptm.h"
#include "pxi/pxi.h"
#include "qtm/qtm.h"
#include "service.h"
#include "sm/sm.h"
#include "sm/srv.h"
#include "soc/soc_u.h"
#include "ssl/ssl_c.h"
#include "../../loader/loader.h"

namespace Service {

const std::array<ServiceModuleInfo, 41> service_module_map{
    {{"FS", 0x00040130'00001102, FS::InstallInterfaces, false},
     {"PM", 0x00040130'00001202, PM::InstallInterfaces, false},
     {"LDR", 0x00040130'00003702, LDR::InstallInterfaces, false},
     {"PXI", 0x00040130'00001402, PXI::InstallInterfaces, false},

     {"ERR", 0x00040030'00008A02, ERR::InstallInterfaces, false},
     {"AC", 0x00040130'00002402, AC::InstallInterfaces, false},
     {"ACT", 0x00040130'00003802, ACT::InstallInterfaces, true},
     {"AM", 0x00040130'00001502, AM::InstallInterfaces, false},
     {"BOSS", 0x00040130'00003402, BOSS::InstallInterfaces, true},
     {"CAM", 0x00040130'00001602,
      [](Core::System& system) {
          CAM::InstallInterfaces(system);
          Y2R::InstallInterfaces(system);
      },
      false},
     {"CECD", 0x00040130'00002602, CECD::InstallInterfaces, true},
     {"CFG", 0x00040130'00001702, CFG::InstallInterfaces, false},
     {"DLP", 0x00040130'00002802, DLP::InstallInterfaces, false},
     {"DSP", 0x00040130'00001A02, DSP::InstallInterfaces, false},
     {"FRD", 0x00040130'00003202, FRD::InstallInterfaces, true},
     {"GSP", 0x00040130'00001C02, GSP::InstallInterfaces, false},
     {"HID", 0x00040130'00001D02, HID::InstallInterfaces, false},
     {"IR", 0x00040130'00003302, IR::InstallInterfaces, false},
     {"MIC", 0x00040130'00002002, MIC::InstallInterfaces, false},
     {"MVD", 0x00040130'20004102, MVD::InstallInterfaces, false},
     {"NDM", 0x00040130'00002B02, NDM::InstallInterfaces, false},
     {"NEWS", 0x00040130'00003502, NEWS::InstallInterfaces, false},
     {"NFC", 0x00040130'00004002, NFC::InstallInterfaces, false},
     {"NIM", 0x00040130'00002C02, NIM::InstallInterfaces, true},
     {"NS", 0x00040130'00008002, APT::InstallInterfaces, false},
     {"NWM", 0x00040130'00002D02, NWM::InstallInterfaces, false},
     {"PTM", 0x00040130'00002202, PTM::InstallInterfaces, false},
     {"QTM", 0x00040130'00004202, QTM::InstallInterfaces, false},
     {"CSND", 0x00040130'00002702, CSND::InstallInterfaces, false},
     {"HTTP", 0x00040130'00002902, HTTP::InstallInterfaces, false},
     {"SOC", 0x00040130'00002E02, SOC::InstallInterfaces, false},
     {"SSL", 0x00040130'00002F02, SSL::InstallInterfaces, false},
     {"PS", 0x00040130'00003102, PS::InstallInterfaces, false},
     {"PLGLDR", 0x00040130'00006902, PLGLDR::InstallInterfaces, false},
     {"MCU", 0x00040130'00001F02, MCU::InstallInterfaces, false},
     // no HLE implementation
     {"CDC", 0x00040130'00001802, nullptr, false},
     {"GPIO", 0x00040130'00001B02, nullptr, false},
     {"I2C", 0x00040130'00001E02, nullptr, false},
     {"MP", 0x00040130'00002A02, nullptr, false},
     {"PDN", 0x00040130'00002102, nullptr, false},
     {"SPI", 0x00040130'00002302, nullptr, false}}};

/**
 * Creates a function string for logging, complete with the name (or header code, depending
 * on what's passed in) the port name, and all the cmd_buff arguments.
 */
[[maybe_unused]] static std::string MakeFunctionString(std::string_view name,
                                                       std::string_view port_name,
                                                       const u32* cmd_buff) {
    // Number of params == bits 0-5 + bits 6-11
    int num_params = (cmd_buff[0] & 0x3F) + ((cmd_buff[0] >> 6) & 0x3F);

    std::string function_string =
        StringFromFormat("function '%s': port=%s", std::string(name).c_str(),
                         std::string(port_name).c_str());
    for (int i = 1; i <= num_params; ++i) {
        function_string += StringFromFormat(", cmd_buff[%d]=0x%X", i, cmd_buff[i]);
    }
    return function_string;
}

ServiceFrameworkBase::ServiceFrameworkBase(const char* service_name, u32 max_sessions,
                                           InvokerFn* handler_invoker)
    : service_name(service_name), max_sessions(max_sessions), handler_invoker(handler_invoker) {}

ServiceFrameworkBase::~ServiceFrameworkBase() = default;

void ServiceFrameworkBase::InstallAsService(SM::ServiceManager& service_manager) {
    std::shared_ptr<Kernel::ServerPort> port;
    R_ASSERT(service_manager.RegisterService(std::addressof(port), service_name, max_sessions));
    port->SetHleHandler(shared_from_this());
}

void ServiceFrameworkBase::InstallAsNamedPort(Kernel::KernelSystem& kernel) {
    auto [server_port, client_port] = kernel.CreatePortPair(max_sessions, service_name);
    server_port->SetHleHandler(shared_from_this());
    kernel.AddNamedPort(service_name, std::move(client_port));
}

void ServiceFrameworkBase::RegisterHandlersBase(const FunctionInfoBase* functions, std::size_t n) {
    handlers.reserve(handlers.size() + n);
    for (std::size_t i = 0; i < n; ++i) {
        // Usually this array is sorted by id already, so hint to insert at the end
        handlers.emplace_hint(handlers.cend(), functions[i].command_id, functions[i]);
    }
}

void ServiceFrameworkBase::ReportUnimplementedFunction(u32* cmd_buf, const FunctionInfoBase* info) {
    IPC::Header header{cmd_buf[0]};
    int num_params = header.normal_params_size + header.translate_params_size;
    std::string function_name = info == nullptr ? StringFromFormat("0x%08x", cmd_buf[0])
                                                 : std::string(info->name);

    std::string result = StringFromFormat(
        "function '%s': port='%s' cmd_buf={[0]=0x%x (0x%04X, %u, %u)", function_name.c_str(),
        service_name, header.raw, header.command_id.Value(), header.normal_params_size.Value(),
        header.translate_params_size.Value());
    for (int i = 1; i <= num_params; ++i) {
        result += StringFromFormat(", [%d]=0x%x", i, cmd_buf[i]);
    }

    result.push_back('}');

    LOG_ERROR(Service, "unknown / unimplemented {}", result);
    // TODO(bunnei): Hack - ignore error
    header.normal_params_size.Assign(1);
    header.translate_params_size.Assign(0);
    cmd_buf[0] = header.raw;
    cmd_buf[1] = 0;
}

void ServiceFrameworkBase::HandleSyncRequest(Kernel::HLERequestContext& context) {
    auto itr = handlers.find(context.CommandHeader().command_id.Value());
    const FunctionInfoBase* info = itr == handlers.end() ? nullptr : &itr->second;
    if (info == nullptr || !info->implemented) {
        context.ReportUnimplemented();
        return ReportUnimplementedFunction(context.CommandBuffer(), info);
    }

    LOG_TRACE(Service, "{}",
              MakeFunctionString(info->name, GetServiceName(), context.CommandBuffer()));
    handler_invoker(this, info->handler_callback, context);
}

std::string ServiceFrameworkBase::GetFunctionName(IPC::Header header) const {
    auto itr = handlers.find(header.command_id.Value());
    if (itr == handlers.end()) {
        return "";
    }

    return itr->second.name;
}

static bool AttemptLLE(const ServiceModuleInfo& service_module, u64 loading_titleid) {
    const bool enable_recommended_lle_modules = Common::Hacks::hack_manager.OverrideBooleanSetting(
        Common::Hacks::HackType::ONLINE_LLE_REQUIRED, loading_titleid,
        Settings::values.enable_required_online_lle_modules.GetValue());

    if (!Settings::values.lle_modules.at(service_module.name) &&
        (!enable_recommended_lle_modules || !service_module.is_online_recommended))
        return false;
    std::unique_ptr<Loader::AppLoader> loader =
        Loader::GetLoader(AM::GetTitleContentPath(FS::MediaType::NAND, service_module.title_id));
    if (!loader) {
        LOG_ERROR(Service,
                  "Service module \"{}\" could not be loaded; Defaulting to HLE implementation.",
                  service_module.name);
        return false;
    }
    std::shared_ptr<Kernel::Process> process;
    Loader::ResultStatus load_result = loader->Load(process);
    if (load_result != Loader::ResultStatus::Success) {
        LOG_ERROR(Service,
                  "Service module \"{}\" could not be loaded (ResultStatus={}); Defaulting to HLE "
                  "implementation.",
                  service_module.name, static_cast<int>(load_result));
        return false;
    }
    LOG_DEBUG(Service, "Service module \"{}\" has been successfully loaded.", service_module.name);
    return true;
}

/// Initialize ServiceManager
void Init(Core::System& core, u64 loading_titleid, std::vector<u64>& lle_modules, bool allow_lle) {
    SM::ServiceManager::InstallInterfaces(core);
    core.Kernel().SetAppMainThreadExtendedSleep(false);
    bool lle_module_present = false;

    for (const auto& service_module : service_module_map) {
        if (core.GetSaveStateStatus() == Core::System::SaveStateStatus::LOADING &&
            std::find(lle_modules.begin(), lle_modules.end(), service_module.title_id) !=
                lle_modules.end()) {
            // The system module has already been loaded before, do not attempt to load again as the
            // process, threads, etc are already serialized in the kernel structures.
            lle_module_present |= true;
            continue;
        }

        const bool has_lle = allow_lle &&
                             core.GetSaveStateStatus() != Core::System::SaveStateStatus::LOADING &&
                             AttemptLLE(service_module, loading_titleid);
        if (has_lle) {
            lle_modules.push_back(service_module.title_id);
        }
        if (!has_lle && service_module.init_function != nullptr) {
            service_module.init_function(core);
        }
        lle_module_present |= has_lle;
    }
    if (lle_module_present) {
        // If there is at least one LLE module, tell the kernel to
        // add a extended sleep to the app main thread (if option enabled).
        core.Kernel().SetAppMainThreadExtendedSleep(true);
    }
    LOG_DEBUG(Service, "initialized OK");
}

} // namespace Service
