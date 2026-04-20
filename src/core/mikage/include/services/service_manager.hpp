#pragma once
#include <array>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>

#include "../kernel/kernel_types.hpp"
#include "../logger.hpp"
#include "../script_manager.hpp"
#include "../memory.hpp"
#include "./ac.hpp"
#include "./act.hpp"
#include "./am.hpp"
#include "./apt.hpp"
#include "./boss.hpp"
#include "./cam.hpp"
#include "./cecd.hpp"
#include "./cfg.hpp"
#include "./csnd.hpp"
#include "./dlp_srvr.hpp"
#include "./dsp.hpp"
#include "./frd.hpp"
#include "./fs.hpp"
#include "./gsp_gpu.hpp"
#include "./gsp_lcd.hpp"
#include "./hid.hpp"
#include "./http.hpp"
#include "./ir/ir_user.hpp"
#include "./ldr_ro.hpp"
#include "./mcu/mcu_hwc.hpp"
#include "./mic.hpp"
#include "./ndm.hpp"
#include "./news_u.hpp"
#include "./nfc.hpp"
#include "./nim.hpp"
#include "./ns.hpp"
#include "./nwm_uds.hpp"
#include "./ptm.hpp"
#include "./service_intercept.hpp"
#include "./soc.hpp"
#include "./ssl.hpp"
#include "./y2r.hpp"

struct EmulatorConfig;
// More circular dependencies!!
class Kernel;

class ServiceManager {
	using Handle = HorizonHandle;

	std::span<u32, 16> regs;
	Memory& mem;
	Kernel& kernel;

	std::optional<Handle> notificationSemaphore;

	MAKE_LOG_FUNCTION(log, srvLogger)

	ACService ac;
	ACTService act;
	AMService am;
	APTService apt;
	BOSSService boss;
	CAMService cam;
	CECDService cecd;
	CFGService cfg;
	CSNDService csnd;
	DlpSrvrService dlp_srvr;
	DSPService dsp;
	HIDService hid;
	HTTPService http;
	IRUserService ir_user;
	FRDService frd;
	FSService fs;
	GPUService gsp_gpu;
	LCDService gsp_lcd;
	LDRService ldr;
	MICService mic;
	NDMService ndm;
	NewsUService news_u;
	NFCService nfc;
	NwmUdsService nwm_uds;
	NIMService nim;
	NSService ns;
	PTMService ptm;
	SOCService soc;
	SSLService ssl;
	Y2RService y2r;

	MCU::HWCService mcu_hwc;

	// Optional scripts can intercept service calls and run code on SyncRequests
	// For example, if we want to intercept dsp::DSP ReadPipe (Header: 0x000E00C0), the "serviceName" field would be "dsp::DSP"
	// and the "function" field would be 0x000E00C0
	ScriptManager& scriptManager;

	// Map from service intercept entries to their corresponding script callbacks
	std::unordered_map<InterceptedService, int> interceptedServices = {};
	// Calling std::unordered_map<T>::size() compiles to a non-trivial function call on Clang, so we store this
	// separately and check it on service calls, for performance reasons
	bool haveServiceIntercepts = false;

	// Checks for whether a service call is intercepted by the script manager and handles it. Returns true if the script manager told us not to handle the function,
	// or false if we should handle it as normal
	bool checkForIntercept(u32 messagePointer, Handle handle);

	// "srv:" commands
	void enableNotification(u32 messagePointer);
	void getServiceHandle(u32 messagePointer);
	void publishToSubscriber(u32 messagePointer);
	void receiveNotification(u32 messagePointer);
	void registerClient(u32 messagePointer);
	void subscribe(u32 messagePointer);
	void unsubscribe(u32 messagePointer);

  public:
	ServiceManager(std::span<u32, 16> regs, Memory& mem, GPU& gpu, u32& currentPID, Kernel& kernel, const EmulatorConfig& config, ScriptManager& scriptManager);
	void reset();
	void initializeFS() { fs.initializeFilesystem(); }
	void handleSyncRequest(u32 messagePointer);

	// Forward a SendSyncRequest IPC message to the service with the respective handle
	void sendCommandToService(u32 messagePointer, Handle handle);

	// Wrappers for communicating with certain services
	void sendGPUInterrupt(GPUInterrupt type) { gsp_gpu.requestInterrupt(type); }
	void setGSPSharedMem(u8* ptr) { gsp_gpu.setSharedMem(ptr); }
	void setHIDSharedMem(u8* ptr) { hid.setSharedMem(ptr); }
	void setCSNDSharedMem(u8* ptr) { csnd.setSharedMemory(ptr); }

	// Input function wrappers
	HIDService& getHID() { return hid; }
	NFCService& getNFC() { return nfc; }
	DSPService& getDSP() { return dsp; }
	Y2RService& getY2R() { return y2r; }
	IRUserService& getIRUser() { return ir_user; }

	void addServiceIntercept(const std::string& service, u32 function, int callbackRef) {
		auto success = interceptedServices.try_emplace(InterceptedService(service, function), callbackRef);
		if (!success.second) {
			// An intercept for this service function already exists
			// Remove the old callback and set the new one
			scriptManager.removeInterceptedService(service, function, success.first->second);
			success.first->second = callbackRef;
		}

		haveServiceIntercepts = true;
	}

	void clearServiceIntercepts() {
		for (const auto& [interceptedService, callbackRef] : interceptedServices) {
			scriptManager.removeInterceptedService(interceptedService.serviceName, interceptedService.function, callbackRef);
		}

		interceptedServices.clear();
		haveServiceIntercepts = false;
	}
};
