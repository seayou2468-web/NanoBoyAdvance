#include "../../../include/services/service_manager.hpp"

#include <set>

#include "../../../include/ipc.hpp"
#include "../../../include/kernel/kernel.hpp"
#include "../../../include/services/service_map.hpp"

ServiceManager::ServiceManager(
	std::span<u32, 16> regs, Memory& mem, GPU& gpu, u32& currentPID, Kernel& kernel, const EmulatorConfig& config
)
	: regs(regs), mem(mem), kernel(kernel), ac(mem), am(mem), boss(mem), act(mem), apt(mem, kernel), cam(mem, kernel), cecd(mem, kernel),
	  cfg(mem, config), csnd(mem, kernel), dlp_srvr(mem), dsp(mem, kernel, config), err_f(mem), hid(mem, kernel), http(mem),
	  ir_user(mem, hid, config, kernel),
		  frd(mem), fs(mem, kernel, config), gsp_gpu(mem, gpu, kernel, currentPID), gsp_lcd(mem), ldr(mem, kernel), mcu_hwc(mem, config),
		  mic(mem, kernel), nfc(mem, kernel), nwm_ext(mem), nim(mem), ndm(mem), news_u(mem), ns(mem), nwm_uds(mem, kernel), ptm(mem, config), pm(mem),
		  pxi_dev(mem), qtm(mem), ps_ps(mem), mvd_std(mem), plgldr(mem), soc(mem), ssl(mem), y2r(mem, kernel), peripheral_bus(mem), os_ext(mem, kernel) {}

static constexpr int MAX_NOTIFICATION_COUNT = 16;

// Reset every single service
void ServiceManager::reset() {
	ac.reset();
	act.reset();
	am.reset();
	apt.reset();
	boss.reset();
	cam.reset();
	cecd.reset();
	cfg.reset();
	csnd.reset();
	dlp_srvr.reset();
	dsp.reset();
	err_f.reset();
	hid.reset();
	http.reset();
	ir_user.reset();
	frd.reset();
	fs.reset();
	gsp_gpu.reset();
	gsp_lcd.reset();
	ldr.reset();
	mcu_hwc.reset();
	mic.reset();
	ndm.reset();
	news_u.reset();
	nfc.reset();
	nwm_ext.reset();
	nim.reset();
	ns.reset();
	peripheral_bus.reset();
	os_ext.reset();
	ptm.reset();
	pm.reset();
	pxi_dev.reset();
	qtm.reset();
	ps_ps.reset();
	mvd_std.reset();
	plgldr.reset();
	soc.reset();
	ssl.reset();
	y2r.reset();

	notificationSemaphore = std::nullopt;
	pendingNotifications.clear();
	subscribedNotifications.clear();
	userRegisteredServices.clear();
	userRegisteredPorts.clear();
}

// Match IPC messages to a "srv:" command based on their header
namespace Commands {
	enum : u32 {
		RegisterClient = 0x00010002,
		EnableNotification = 0x00020000,
		RegisterService = 0x00030100,
		UnregisterService = 0x000400C0,
		GetServiceHandle = 0x00050100,
		RegisterPort = 0x000600C2,
		UnregisterPort = 0x000700C0,
		GetPort = 0x00080100,
		Subscribe = 0x00090040,
		Unsubscribe = 0x000A0040,
		ReceiveNotification = 0x000B0000,
		PublishToSubscriber = 0x000C0080,
		PublishAndGetSubscriber = 0x000D0040,
		IsServiceRegistered = 0x000E00C0
	};
}

namespace {
std::optional<std::string> readIPCName(Memory& mem, u32 pointer, u32 nameLength) {
	std::array<char, 8> rawName{};
	for (u32 i = 0; i < rawName.size(); i++) {
		rawName[i] = static_cast<char>(mem.read8(pointer + i));
	}

	u32 nulTerminator = 8;
	for (u32 i = 0; i < rawName.size(); i++) {
		if (rawName[i] == '\0') {
			nulTerminator = i;
			break;
		}
	}

	// Some titles pass unreliable nameLength values for SRV commands.
	// Fall back to a nul-terminated parse instead of rejecting outright.
	u32 effectiveLength = nameLength;
	if (effectiveLength == 0 || effectiveLength > 8) {
		effectiveLength = nulTerminator;
	}

	if (effectiveLength == 0 || effectiveLength > 8) {
		return std::nullopt;
	}

	std::string name(rawName.data(), rawName.data() + effectiveLength);
	return name;
}
}

// Handle an IPC message issued using the SendSyncRequest SVC
// The parameters are stored in thread-local storage in this format: https://www.3dbrew.org/wiki/IPC#Message_Structure
// messagePointer: The base pointer for the IPC message
void ServiceManager::handleSyncRequest(u32 messagePointer) {
	const u32 header = mem.read32(messagePointer);

	switch (header) {
		case Commands::EnableNotification: enableNotification(messagePointer); break;
		case Commands::ReceiveNotification: receiveNotification(messagePointer); break;
		case Commands::RegisterClient: registerClient(messagePointer); break;
		case Commands::RegisterService: registerService(messagePointer); break;
		case Commands::UnregisterService: unregisterService(messagePointer); break;
		case Commands::GetServiceHandle: getServiceHandle(messagePointer); break;
		case Commands::Subscribe: subscribe(messagePointer); break;
		case Commands::Unsubscribe: unsubscribe(messagePointer); break;
		case Commands::PublishToSubscriber: publishToSubscriber(messagePointer); break;
		case Commands::RegisterPort: registerPort(messagePointer); break;
		case Commands::UnregisterPort: unregisterPort(messagePointer); break;
		case Commands::GetPort: getPort(messagePointer); break;
		case Commands::PublishAndGetSubscriber: publishAndGetSubscriber(messagePointer); break;
		case Commands::IsServiceRegistered: isServiceRegistered(messagePointer); break;
		default: {
			Helpers::warn("Unknown \"srv:\" command: %08X", header);
			mem.write32(messagePointer, IPC::responseHeader(header >> 16, 1, 0));
			mem.write32(messagePointer + 4, Result::OS::NotImplemented);
			break;
		}
	}
}

// https://www.3dbrew.org/wiki/SRV:RegisterClient
void ServiceManager::registerClient(u32 messagePointer) {
	const u32 pidDescriptor = mem.read32(messagePointer + 4);
	log("srv::registerClient(pid descriptor = %08X)\n", pidDescriptor);

	// IPC CallingPidDesc, see IPC::RequestParser behaviour in reference HLE.
	if (pidDescriptor != 0x20) {
		mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 0));
		mem.write32(messagePointer + 4, Result::OS::InvalidCombination);
		return;
	}

	mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

// clang-format off
static const ServiceMapEntry serviceMapArray[] = {
	{ "ac:u", KernelHandles::AC },
	{ "ac:i", KernelHandles::AC },
	{ "act:a", KernelHandles::ACT },
	{ "act:u", KernelHandles::ACT },
	{ "am:app", KernelHandles::AM },
	{ "am:net", KernelHandles::AM },
	{ "am:sys", KernelHandles::AM },
	{ "am:u", KernelHandles::AM },
	{ "APT:S", KernelHandles::APT }, // TODO: APT:A, APT:S and APT:U are slightly different
	{ "APT:A", KernelHandles::APT },
	{ "APT:U", KernelHandles::APT },
	{ "boss:U", KernelHandles::BOSS },
	{ "boss:P", KernelHandles::BOSS },
	{ "cam:u", KernelHandles::CAM },
	{ "cam:c", KernelHandles::CAM },
	{ "cam:q", KernelHandles::CAM },
	{ "cam:s", KernelHandles::CAM },
	{ "cecd:u", KernelHandles::CECD },
	{ "cecd:s", KernelHandles::CECD },
	{ "cecd:ndm", KernelHandles::CECD },
	{ "cfg:u", KernelHandles::CFG_U },
	{ "cfg:i", KernelHandles::CFG_I },
	{ "cfg:s", KernelHandles::CFG_S },
	{ "cfg:nor", KernelHandles::CFG_NOR },
	{ "csnd:SND", KernelHandles::CSND },
	{ "dlp:SRVR", KernelHandles::DLP_SRVR },
	{ "dlp:CLNT", KernelHandles::DLP_CLNT },
	{ "dlp:FKCL", KernelHandles::DLP_FKCL },
	{ "dsp::DSP", KernelHandles::DSP },
	{ "err:f", KernelHandles::ERR_F },
	{ "hid:USER", KernelHandles::HID },
	{ "hid:SPVR", KernelHandles::HID },
	{ "http:C", KernelHandles::HTTP },
	{ "ir:USER", KernelHandles::IR_USER },
	{ "ir:rst", KernelHandles::IR_USER },
	{ "ir:u", KernelHandles::IR_USER },
	{ "frd:a", KernelHandles::FRD_A },
	{ "frd:u", KernelHandles::FRD_U },
	{ "fs:USER", KernelHandles::FS },
	{ "gsp::Gpu", KernelHandles::GPU },
	{ "gsp::GPU", KernelHandles::GPU },
	{ "GSP::GPU", KernelHandles::GPU },
	{ "gsp::Lcd", KernelHandles::LCD },
	{ "gsp::LCD", KernelHandles::LCD },
	{ "GSP::LCD", KernelHandles::LCD },
	{ "gx:low", KernelHandles::GPU },
	{ "GX:low", KernelHandles::GPU },
	{ "ldr:ro", KernelHandles::LDR_RO },
	{ "mcu::HWC", KernelHandles::MCU_HWC },
	{ "mic:u", KernelHandles::MIC },
	{ "ndm:u", KernelHandles::NDM },
	{ "news:u", KernelHandles::NEWS_U },
	{ "news:s", KernelHandles::NEWS_S },
	{ "nfc:u", KernelHandles::NFC },
	{ "nfc:m", KernelHandles::NFC },
	{ "ns:s", KernelHandles::NS_S },
	{ "ns:c", KernelHandles::NS_C },
	{ "ns:p", KernelHandles::NS_C },
	{ "nwm::EXT", KernelHandles::NWM_EXT },
	{ "nwm::CEC", KernelHandles::NWM_CEC },
	{ "nwm::INF", KernelHandles::NWM_INF },
	{ "nwm::SAP", KernelHandles::NWM_SAP },
	{ "nwm::SOC", KernelHandles::NWM_SOC },
	{ "nwm::TST", KernelHandles::NWM_TST },
	{ "nwm::UDS", KernelHandles::NWM_UDS },
	{ "nim:aoc", KernelHandles::NIM },
	{ "nim:s", KernelHandles::NIM },
	{ "nim:u", KernelHandles::NIM },
	{ "ptm:u", KernelHandles::PTM_U }, // TODO: ptm:u and ptm:sysm have very different command sets
	{ "ptm:s", KernelHandles::PTM_SYSM },
	{ "ptm:sysm", KernelHandles::PTM_SYSM },
	{ "ptm:play", KernelHandles::PTM_PLAY },
	{ "ptm:gets", KernelHandles::PTM_GETS },
	{ "ptm:sets", KernelHandles::PTM_SETS },
	{ "soc:U", KernelHandles::SOC },
	{ "ssl:C", KernelHandles::SSL },
	{ "y2r:u", KernelHandles::Y2R },
	{ "mcu::RTC", KernelHandles::MCU_HWC },
	{ "plg:ldr", KernelHandles::PLG_LDR },
	{ "mvd:std", KernelHandles::MVD_STD },
	{ "ps:ps", KernelHandles::PS_PS },
	{ "pxi:dev", KernelHandles::PXI_DEV },
	{ "pm:app", KernelHandles::PM_APP },
	{ "pm:dbg", KernelHandles::PM_DBG },
	{ "qtm:c", KernelHandles::QTM_C },
	{ "qtm:s", KernelHandles::QTM_S },
	{ "qtm:sp", KernelHandles::QTM_SP },
	{ "qtm:u", KernelHandles::QTM_U },
	{ "cdc:U", KernelHandles::CDC_U },
	{ "cdc:u", KernelHandles::CDC_U },
	{ "gpio:U", KernelHandles::GPIO_U },
	{ "gpio:u", KernelHandles::GPIO_U },
	{ "i2c:U", KernelHandles::I2C_U },
	{ "i2c:u", KernelHandles::I2C_U },
	{ "mp:U", KernelHandles::MP_U },
	{ "mp:u", KernelHandles::MP_U },
	{ "pdn:U", KernelHandles::PDN_U },
	{ "pdn:u", KernelHandles::PDN_U },
	{ "spi:U", KernelHandles::SPI_U },
	{ "spi:u", KernelHandles::SPI_U },
};
// clang-format on

static std::set<ServiceMapEntry, ServiceMapByNameComparator> serviceMapByName{std::begin(serviceMapArray), std::end(serviceMapArray)};

// https://www.3dbrew.org/wiki/SRV:GetServiceHandle
void ServiceManager::getServiceHandle(u32 messagePointer) {
	u32 nameLength = mem.read32(messagePointer + 12);
	u32 flags = mem.read32(messagePointer + 16);
	u32 handle = 0;
	const std::optional<std::string> serviceOpt = readIPCName(mem, messagePointer + 4, nameLength);

	if (!serviceOpt.has_value()) {
		mem.write32(messagePointer, IPC::responseHeader(0x5, 1, 2));
		mem.write32(messagePointer + 4, Result::OS::InvalidCombination);
		mem.write32(messagePointer + 8, 0);
		mem.write32(messagePointer + 12, 0);
		return;
	}

	const std::string& service = serviceOpt.value();
	log("srv::getServiceHandle (Service: %s, nameLength: %d, flags: %d)\n", service.c_str(), nameLength, flags);

	// Look up service handle in map, panic if it does not exist
	if (auto search = serviceMapByName.find(service); search != serviceMapByName.end()) {
		handle = search->second;
		mem.write32(messagePointer, IPC::responseHeader(0x5, 1, 2));
		mem.write32(messagePointer + 4, Result::Success);
		mem.write32(messagePointer + 8, 0x04000000);
		mem.write32(messagePointer + 12, handle);
		return;
	}

	// Allow dynamically registered services to appear as valid without crashing.
	// We don't expose server sessions yet, so return a graceful NotImplemented result.
	if (auto it = userRegisteredServices.find(service); it != userRegisteredServices.end()) {
		mem.write32(messagePointer, IPC::responseHeader(0x5, 1, 2));
		mem.write32(messagePointer + 4, Result::Success);
		mem.write32(messagePointer + 8, 0x04000000);
		mem.write32(messagePointer + 12, kernel.makePortSession(it->second));
		return;
	}

	// Keep unknown-but-valid service names alive behind a generic stub.
	// Some homebrew modules request names like "$VT!" that do not contain ':'.
	const bool valid_service_len = service.length() <= 8 && !service.empty();
	if (valid_service_len) {
		mem.write32(messagePointer, IPC::responseHeader(0x5, 1, 2));
		mem.write32(messagePointer + 4, Result::Success);
		mem.write32(messagePointer + 8, 0x04000000);
		mem.write32(messagePointer + 12, KernelHandles::OS_STUB);
		return;
	}

	mem.write32(messagePointer, IPC::responseHeader(0x5, 1, 2));
	mem.write32(messagePointer + 4, Result::OS::NotImplemented);
	mem.write32(messagePointer + 8, 0);
	mem.write32(messagePointer + 12, 0);
}

void ServiceManager::enableNotification(u32 messagePointer) {
	log("srv::EnableNotification()\n");

	// Make a semaphore for notifications if none exists currently
	if (!notificationSemaphore.has_value() || kernel.getObject(notificationSemaphore.value(), KernelObjectType::Semaphore) == nullptr) {
		notificationSemaphore = kernel.makeSemaphore(0, MAX_NOTIFICATION_COUNT);
	}

	mem.write32(messagePointer, IPC::responseHeader(0x2, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);  // Result code
	mem.write32(messagePointer + 8, 0x04000000);       // Translation descriptor (copy handle, 1 handle)
	// Handle to semaphore signaled on process notification
	mem.write32(messagePointer + 12, notificationSemaphore.value());
}

void ServiceManager::receiveNotification(u32 messagePointer) {
	log("srv::ReceiveNotification()\n");

	u32 notificationID = 0;
	if (!pendingNotifications.empty()) {
		notificationID = pendingNotifications.front();
		pendingNotifications.pop_front();
	}

	mem.write32(messagePointer, IPC::responseHeader(0xB, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);  // Result code
	mem.write32(messagePointer + 8, notificationID);   // Notification ID
}

void ServiceManager::subscribe(u32 messagePointer) {
	u32 id = mem.read32(messagePointer + 4);
	log("srv::Subscribe (id = %d)\n", id);
	subscribedNotifications.insert(id);

	mem.write32(messagePointer, IPC::responseHeader(0x9, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void ServiceManager::unsubscribe(u32 messagePointer) {
	u32 id = mem.read32(messagePointer + 4);
	log("srv::Unsubscribe (id = %d)\n", id);
	subscribedNotifications.erase(id);

	mem.write32(messagePointer, IPC::responseHeader(0xA, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void ServiceManager::publishToSubscriber(u32 messagePointer) {
	u32 id = mem.read32(messagePointer + 4);
	u32 flags = mem.read32(messagePointer + 8);
	log("srv::PublishToSubscriber (Notification ID = %d, flags = %08X)\n", id, flags);

	if (subscribedNotifications.contains(id) && pendingNotifications.size() < MAX_NOTIFICATION_COUNT) {
		pendingNotifications.push_back(id);
		if (notificationSemaphore.has_value()) {
			kernel.releaseSemaphore(notificationSemaphore.value(), 1);
		}
	}

	mem.write32(messagePointer, IPC::responseHeader(0xC, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void ServiceManager::registerService(u32 messagePointer) {
	const u32 nameLength = mem.read32(messagePointer + 12);
	const u32 maxSessions = mem.read32(messagePointer + 16);
	const std::optional<std::string> serviceOpt = readIPCName(mem, messagePointer + 4, nameLength);
	const char* serviceName = serviceOpt.has_value() ? serviceOpt->c_str() : "<invalid>";
	log("srv::RegisterService (service = %s, nameLength = %d, maxSessions = %d)\n", serviceName, nameLength, maxSessions);

	if (!serviceOpt.has_value() || maxSessions == 0) {
		mem.write32(messagePointer, IPC::responseHeader(0x3, 1, 2));
		mem.write32(messagePointer + 4, Result::OS::InvalidCombination);
		mem.write32(messagePointer + 8, 0);
		mem.write32(messagePointer + 12, 0);
		return;
	}
	const std::string& service = serviceOpt.value();

	// Built-in service names cannot be overridden.
	if (serviceMapByName.find(service) == serviceMapByName.end()) {
		if (!userRegisteredServices.contains(service)) {
			userRegisteredServices.emplace(service, kernel.makeNamedPort(service.c_str()));
		}
	}

	mem.write32(messagePointer, IPC::responseHeader(0x3, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0x04000000);  // Translation descriptor (copy handle, 1 handle)
	if (auto it = userRegisteredServices.find(service); it != userRegisteredServices.end()) {
		mem.write32(messagePointer + 12, it->second);
	} else {
		mem.write32(messagePointer + 12, 0);
	}
}

void ServiceManager::unregisterService(u32 messagePointer) {
	const u32 nameLength = mem.read32(messagePointer + 12);
	const std::optional<std::string> serviceOpt = readIPCName(mem, messagePointer + 4, nameLength);
	if (!serviceOpt.has_value()) {
		mem.write32(messagePointer, IPC::responseHeader(0x4, 1, 0));
		mem.write32(messagePointer + 4, Result::OS::InvalidCombination);
		return;
	}

	const std::string& service = serviceOpt.value();
	log("srv::UnregisterService (service = %s)\n", service.c_str());
	userRegisteredServices.erase(service);

	mem.write32(messagePointer, IPC::responseHeader(0x4, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void ServiceManager::isServiceRegistered(u32 messagePointer) {
	const u32 nameLength = mem.read32(messagePointer + 12);
	const std::optional<std::string> serviceOpt = readIPCName(mem, messagePointer + 4, nameLength);
	if (!serviceOpt.has_value()) {
		mem.write32(messagePointer, IPC::responseHeader(0xE, 1, 0));
		mem.write32(messagePointer + 4, Result::OS::InvalidCombination);
		return;
	}

	const std::string& service = serviceOpt.value();
	bool registered = serviceMapByName.find(service) != serviceMapByName.end() || userRegisteredServices.find(service) != userRegisteredServices.end();
	log("srv::IsServiceRegistered (service = %s, registered = %d)\n", service.c_str(), registered ? 1 : 0);

	mem.write32(messagePointer, IPC::responseHeader(0xE, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, registered ? 1 : 0);
}

void ServiceManager::registerPort(u32 messagePointer) {
	const u32 nameLength = mem.read32(messagePointer + 12);
	const u32 maxSessions = mem.read32(messagePointer + 16);
	const std::optional<std::string> portOpt = readIPCName(mem, messagePointer + 4, nameLength);
	const char* portName = portOpt.has_value() ? portOpt->c_str() : "<invalid>";
	log("srv::RegisterPort (port = %s, nameLength = %u, maxSessions = %u)\n", portName, nameLength, maxSessions);

	if (!portOpt.has_value() || maxSessions == 0) {
		mem.write32(messagePointer, IPC::responseHeader(0x6, 1, 2));
		mem.write32(messagePointer + 4, Result::OS::InvalidCombination);
		mem.write32(messagePointer + 8, 0);
		mem.write32(messagePointer + 12, 0);
		return;
	}
	const std::string& port = portOpt.value();

	if (!userRegisteredPorts.contains(port)) {
		userRegisteredPorts.emplace(port, kernel.makeNamedPort(port.c_str()));
	}

	mem.write32(messagePointer, IPC::responseHeader(0x6, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0x04000000); // Translation descriptor (copy handle, 1 handle)
	mem.write32(messagePointer + 12, userRegisteredPorts.at(port));
}

void ServiceManager::unregisterPort(u32 messagePointer) {
	const u32 nameLength = mem.read32(messagePointer + 12);
	const std::optional<std::string> portOpt = readIPCName(mem, messagePointer + 4, nameLength);
	if (!portOpt.has_value()) {
		mem.write32(messagePointer, IPC::responseHeader(0x7, 1, 0));
		mem.write32(messagePointer + 4, Result::OS::InvalidCombination);
		return;
	}

	const std::string& port = portOpt.value();
	log("srv::UnregisterPort (port = %s)\n", port.c_str());
	userRegisteredPorts.erase(port);
	mem.write32(messagePointer, IPC::responseHeader(0x7, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void ServiceManager::getPort(u32 messagePointer) {
	const u32 nameLength = mem.read32(messagePointer + 12);
	const std::optional<std::string> portOpt = readIPCName(mem, messagePointer + 4, nameLength);
	if (!portOpt.has_value()) {
		mem.write32(messagePointer, IPC::responseHeader(0x8, 1, 2));
		mem.write32(messagePointer + 4, Result::OS::InvalidCombination);
		mem.write32(messagePointer + 8, 0);
		mem.write32(messagePointer + 12, 0);
		return;
	}

	const std::string& port = portOpt.value();
	log("srv::GetPort (port = %s)\n", port.c_str());
	mem.write32(messagePointer, IPC::responseHeader(0x8, 1, 2));

	if (auto it = userRegisteredPorts.find(port); it != userRegisteredPorts.end()) {
		mem.write32(messagePointer + 4, Result::Success);
		mem.write32(messagePointer + 8, 0x04000000); // Translation descriptor (copy handle, 1 handle)
		mem.write32(messagePointer + 12, kernel.makePortSession(it->second));
		return;
	}

	mem.write32(messagePointer + 4, Result::OS::NotImplemented);
	mem.write32(messagePointer + 8, 0);
	mem.write32(messagePointer + 12, 0);
}

void ServiceManager::publishAndGetSubscriber(u32 messagePointer) {
	u32 id = mem.read32(messagePointer + 4);
	log("srv::PublishAndGetSubscriber (id = %u)\n", id);
	if (subscribedNotifications.contains(id) && pendingNotifications.size() < MAX_NOTIFICATION_COUNT) {
		pendingNotifications.push_back(id);
		if (notificationSemaphore.has_value()) {
			kernel.releaseSemaphore(notificationSemaphore.value(), 1);
		}
	}

	mem.write32(messagePointer, IPC::responseHeader(0xD, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, subscribedNotifications.contains(id) ? 1 : 0);
}

void ServiceManager::sendCommandToService(u32 messagePointer, Handle handle) {
	switch (handle) {
		// Breaking alphabetical order a bit to place the ones I think are most common at the top
		case KernelHandles::GPU: [[likely]] gsp_gpu.handleSyncRequest(messagePointer); break;
		case KernelHandles::FS: [[likely]] fs.handleSyncRequest(messagePointer); break;
		case KernelHandles::APT: [[likely]] apt.handleSyncRequest(messagePointer); break;
		case KernelHandles::DSP: [[likely]] dsp.handleSyncRequest(messagePointer); break;

		case KernelHandles::AC: ac.handleSyncRequest(messagePointer); break;
		case KernelHandles::ACT: act.handleSyncRequest(messagePointer); break;
		case KernelHandles::AM: am.handleSyncRequest(messagePointer); break;
		case KernelHandles::BOSS: boss.handleSyncRequest(messagePointer); break;
		case KernelHandles::CAM: cam.handleSyncRequest(messagePointer); break;
		case KernelHandles::CECD: cecd.handleSyncRequest(messagePointer); break;
		case KernelHandles::CFG_U: cfg.handleSyncRequest(messagePointer, CFGService::Type::U); break;
		case KernelHandles::CFG_I: cfg.handleSyncRequest(messagePointer, CFGService::Type::I); break;
		case KernelHandles::CFG_S: cfg.handleSyncRequest(messagePointer, CFGService::Type::S); break;
		case KernelHandles::CFG_NOR: cfg.handleSyncRequest(messagePointer, CFGService::Type::NOR); break;
		case KernelHandles::CSND: csnd.handleSyncRequest(messagePointer); break;
		case KernelHandles::DLP_SRVR: dlp_srvr.handleSyncRequest(messagePointer, DlpSrvrService::Type::SRVR); break;
		case KernelHandles::DLP_CLNT: dlp_srvr.handleSyncRequest(messagePointer, DlpSrvrService::Type::CLNT); break;
		case KernelHandles::DLP_FKCL: dlp_srvr.handleSyncRequest(messagePointer, DlpSrvrService::Type::FKCL); break;
		case KernelHandles::CDC_U: peripheral_bus.handleSyncRequest(messagePointer, PeripheralBusService::Type::CDC); break;
		case KernelHandles::GPIO_U: peripheral_bus.handleSyncRequest(messagePointer, PeripheralBusService::Type::GPIO); break;
		case KernelHandles::I2C_U: peripheral_bus.handleSyncRequest(messagePointer, PeripheralBusService::Type::I2C); break;
		case KernelHandles::MP_U: peripheral_bus.handleSyncRequest(messagePointer, PeripheralBusService::Type::MP); break;
		case KernelHandles::PDN_U: peripheral_bus.handleSyncRequest(messagePointer, PeripheralBusService::Type::PDN); break;
		case KernelHandles::SPI_U: peripheral_bus.handleSyncRequest(messagePointer, PeripheralBusService::Type::SPI); break;
		case KernelHandles::ERR_F: err_f.handleSyncRequest(messagePointer); break;
		case KernelHandles::HID: hid.handleSyncRequest(messagePointer); break;
		case KernelHandles::HTTP: http.handleSyncRequest(messagePointer); break;
		case KernelHandles::IR_USER: ir_user.handleSyncRequest(messagePointer); break;
		case KernelHandles::FRD_A: frd.handleSyncRequest(messagePointer, FRDService::Type::A); break;
		case KernelHandles::FRD_U: frd.handleSyncRequest(messagePointer, FRDService::Type::U); break;
		case KernelHandles::LCD: gsp_lcd.handleSyncRequest(messagePointer); break;
		case KernelHandles::LDR_RO: ldr.handleSyncRequest(messagePointer); break;
		case KernelHandles::MCU_HWC: mcu_hwc.handleSyncRequest(messagePointer); break;
		case KernelHandles::MIC: mic.handleSyncRequest(messagePointer); break;
		case KernelHandles::NFC: nfc.handleSyncRequest(messagePointer); break;
		case KernelHandles::NIM: nim.handleSyncRequest(messagePointer); break;
		case KernelHandles::NDM: ndm.handleSyncRequest(messagePointer); break;
		case KernelHandles::NEWS_U: news_u.handleSyncRequest(messagePointer, NewsUService::Type::U); break;
		case KernelHandles::NEWS_S: news_u.handleSyncRequest(messagePointer, NewsUService::Type::S); break;
		case KernelHandles::NS_S: ns.handleSyncRequest(messagePointer, NSService::Type::S); break;
		case KernelHandles::NS_C: ns.handleSyncRequest(messagePointer, NSService::Type::C); break;
		case KernelHandles::NWM_EXT: nwm_ext.handleSyncRequest(messagePointer, NwmExtService::Type::EXT); break;
		case KernelHandles::NWM_CEC: nwm_ext.handleSyncRequest(messagePointer, NwmExtService::Type::CEC); break;
		case KernelHandles::NWM_INF: nwm_ext.handleSyncRequest(messagePointer, NwmExtService::Type::INF); break;
		case KernelHandles::NWM_SAP: nwm_ext.handleSyncRequest(messagePointer, NwmExtService::Type::SAP); break;
		case KernelHandles::NWM_SOC: nwm_ext.handleSyncRequest(messagePointer, NwmExtService::Type::SOC); break;
		case KernelHandles::NWM_TST: nwm_ext.handleSyncRequest(messagePointer, NwmExtService::Type::TST); break;
		case KernelHandles::NWM_UDS: nwm_uds.handleSyncRequest(messagePointer); break;
		case KernelHandles::PTM_PLAY: ptm.handleSyncRequest(messagePointer, PTMService::Type::PLAY); break;
		case KernelHandles::PTM_SYSM: ptm.handleSyncRequest(messagePointer, PTMService::Type::SYSM); break;
		case KernelHandles::PTM_U: ptm.handleSyncRequest(messagePointer, PTMService::Type::U); break;
		case KernelHandles::PTM_GETS: ptm.handleSyncRequest(messagePointer, PTMService::Type::GETS); break;
		case KernelHandles::PTM_SETS: ptm.handleSyncRequest(messagePointer, PTMService::Type::SETS); break;
		case KernelHandles::PM_APP: pm.handleSyncRequest(messagePointer, PMService::Type::App); break;
		case KernelHandles::PM_DBG: pm.handleSyncRequest(messagePointer, PMService::Type::Debug); break;
		case KernelHandles::PXI_DEV: pxi_dev.handleSyncRequest(messagePointer); break;
		case KernelHandles::OS_STUB:
			// First let Aurora's local generic stub try. If the command remains unimplemented,
			// fall back to external bridge if available.
			os_ext.handleSyncRequest(messagePointer);
			if (fallbackBridge != nullptr) {
				const u32 result = mem.read32(messagePointer + 4);
				if (result == Result::OS::NotImplemented) {
					(void)fallbackBridge->handleUnknownService(handle, messagePointer);
				}
			}
			break;
		case KernelHandles::PS_PS: ps_ps.handleSyncRequest(messagePointer); break;
		case KernelHandles::QTM_C: qtm.handleSyncRequest(messagePointer, QTMService::Type::C); break;
		case KernelHandles::QTM_S: qtm.handleSyncRequest(messagePointer, QTMService::Type::S); break;
		case KernelHandles::QTM_SP: qtm.handleSyncRequest(messagePointer, QTMService::Type::SP); break;
		case KernelHandles::QTM_U: qtm.handleSyncRequest(messagePointer, QTMService::Type::U); break;
		case KernelHandles::MVD_STD: mvd_std.handleSyncRequest(messagePointer); break;
		case KernelHandles::PLG_LDR: plgldr.handleSyncRequest(messagePointer); break;
		case KernelHandles::SOC: soc.handleSyncRequest(messagePointer); break;
		case KernelHandles::SSL: ssl.handleSyncRequest(messagePointer); break;
		case KernelHandles::Y2R: y2r.handleSyncRequest(messagePointer); break;
		default: {
			Helpers::warn("Sent IPC message to unknown service %08X Command=%08X", handle, mem.read32(messagePointer));
			if (fallbackBridge != nullptr && fallbackBridge->handleUnknownService(handle, messagePointer)) {
				break;
			}

			mem.write32(messagePointer, IPC::responseHeader(mem.read32(messagePointer) >> 16, 1, 0));
			mem.write32(messagePointer + 4, Result::OS::NotImplemented);
			break;
		}
	}
}

bool ServiceManager::tryFallbackBridgeForPort(std::string_view portName, u32 messagePointer) {
	if (fallbackBridge == nullptr) {
		return false;
	}
	return fallbackBridge->handleUnsupportedPort(portName, messagePointer);
}
