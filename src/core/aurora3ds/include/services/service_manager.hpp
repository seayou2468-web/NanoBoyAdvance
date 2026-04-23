#pragma once
#include <array>
#include <optional>
#include <span>
#include <string>

#include "../kernel/kernel_types.hpp"
#include "../logger.hpp"
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
#include "./err_f.hpp"
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
#include "./nwm_ext.hpp"
#include "./nwm_uds.hpp"
#include "./ptm.hpp"
#include "./pm.hpp"
#include "./pxi_dev.hpp"
#include "./qtm.hpp"
#include "./soc.hpp"
#include "./ssl.hpp"
#include "./y2r.hpp"
#include "./ps_ps.hpp"
#include "./mvd_std.hpp"
#include "./plgldr.hpp"

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
	ErrFService err_f;
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
	NwmExtService nwm_ext;
	NwmUdsService nwm_uds;
	NIMService nim;
	NSService ns;
	PTMService ptm;
	PMService pm;
	PXIDevService pxi_dev;
	QTMService qtm;
	PSPSService ps_ps;
	MVDStdService mvd_std;
	PLGLDRService plgldr;
	SOCService soc;
	SSLService ssl;
	Y2RService y2r;

	MCU::HWCService mcu_hwc;


	// "srv:" commands
	void enableNotification(u32 messagePointer);
	void getServiceHandle(u32 messagePointer);
	void publishToSubscriber(u32 messagePointer);
	void receiveNotification(u32 messagePointer);
	void registerClient(u32 messagePointer);
	void subscribe(u32 messagePointer);
	void unsubscribe(u32 messagePointer);

  public:
	ServiceManager(std::span<u32, 16> regs, Memory& mem, GPU& gpu, u32& currentPID, Kernel& kernel, const EmulatorConfig& config);
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

};
