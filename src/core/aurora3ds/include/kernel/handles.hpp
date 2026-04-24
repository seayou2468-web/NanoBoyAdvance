#pragma once
#include "../helpers.hpp"

using HorizonHandle = u32;

namespace KernelHandles {
	enum : u32 {
		Max = 0xFFFF7FFF, // Max handle the kernel can automagically allocate

		// Hardcoded handles
		CurrentThread = 0xFFFF8000,  // Used by the original kernel
		CurrentProcess = 0xFFFF8001, // Used by the original kernel
		AC,   // Something network related
		ACT,  // Handles NNID accounts
		AM,   // Application manager
		APT,  // App Title something service?
		BOSS, // Streetpass stuff?
		CAM,  // Camera service
		CECD, // More Streetpass stuff?
		CFG_U,  // CFG service (Console & region info)
		CFG_I,
		CFG_S,     // Used by most system apps in lieu of cfg:u
		CFG_NOR,   // Used by system settings app
		CSND,      // Plays audio directly from PCM samples
		DLP_SRVR,  // Download Play: Server. Used for network play.
		DSP,       // DSP service (Used for audio decoding and output)
		HID,       // HID service (Handles input-related things including gyro. Does NOT handle New3DS controls or CirclePadPro)
		HTTP,      // HTTP service (Handles HTTP requests)
		IR_USER,   // One of 3 infrared communication services
		FRD_A,     // Friend service (Miiverse friend service)
		FRD_U,
		FS,        // Filesystem service
		GPU,       // GPU service
		LCD,       // LCD service (Used for configuring the displays)
		LDR_RO,    // Loader service. Used for loading CROs.
		MCU_HWC,   // Used for various MCU hardware-related things like battery control
		MIC,       // MIC service (Controls the microphone)
		NFC,       // NFC (Duh), used for Amiibo
		NIM,       // Updates, DLC, etc
		NDM,       // ?????
		NS_S,      // Nintendo Shell service
		NS_C,      // Nintendo Shell content service
		NWM_EXT,   // nwm::EXT
		NWM_UDS,   // Local multiplayer
		NEWS_S,    // news:u on steroids
		NEWS_U,    // This service literally has 1 command (AddNotification)
		PTM_U,     // PTM service (Used for accessing various console info, such as battery, shell and pedometer state)
		PTM_SYSM,  // PTM system service
		PTM_PLAY,  // PTM Play service, used for retrieving play history
		PTM_GETS,  // PTM RTC service (GetSystemTime)
		PTM_SETS,  // PTM RTC service (SetSystemTime)
		SOC,       // Socket service
		SSL,       // SSL service (Totally didn't expect that)
		Y2R,       // Also does camera stuff
		MVD_STD,   // Video decoder service
		PLG_LDR,   // Plugin loader service
		PM_APP,    // Process manager app service
		PM_DBG,    // Process manager debug service
		PS_PS,     // Platform service
		QTM_C,     // qtm:c calibration
		QTM_S,     // qtm:s
		QTM_SP,    // qtm:sp
		QTM_U,     // qtm:u
		ERR_F,     // Fatal error service
		PXI_DEV,   // PXI developer service
		DLP_CLNT,  // Download Play: Client interface
		DLP_FKCL,  // Download Play: Fake client interface
		CDC_U,     // CDC user service
		GPIO_U,    // GPIO user service
		I2C_U,     // I2C user service
		MP_U,      // MP user service
		PDN_U,     // Power management user service
		SPI_U,     // SPI user service
		NWM_CEC,   // nwm::CEC
		NWM_INF,   // nwm::INF
		NWM_SAP,   // nwm::SAP
		NWM_SOC,   // nwm::SOC
		NWM_TST,   // nwm::TST
		OS_STUB,   // Generic fallback for not-yet-implemented OS service ports

		MinServiceHandle = AC,
		MaxServiceHandle = OS_STUB,
		LegacyMaxServiceHandle = PS_PS,

		GSPSharedMemHandle = LegacyMaxServiceHandle + 1, // Keep shared-memory handles stable
		FontSharedMemHandle,
		CSNDSharedMemHandle,
		APTCaptureSharedMemHandle, // Shared memory for display capture info,  
		HIDSharedMemHandle,

		MinSharedMemHandle = GSPSharedMemHandle,
		MaxSharedMemHandle = HIDSharedMemHandle,
	};

	// Returns whether "handle" belongs to one of the OS services
	static constexpr bool isServiceHandle(HorizonHandle handle) {
		return handle >= MinServiceHandle && handle <= MaxServiceHandle;
	}

	// Returns whether "handle" belongs to one of the OS services' shared memory areas
	static constexpr bool isSharedMemHandle(HorizonHandle handle) {
		return handle >= MinSharedMemHandle && handle <= MaxSharedMemHandle;
	}

	// Returns the name of a handle as a string based on the given handle
	static const char* getServiceName(HorizonHandle handle) {
		switch (handle) {
			case AC: return "AC";
			case ACT: return "ACT";
			case AM: return "AM";
			case APT: return "APT";
			case BOSS: return "BOSS";
			case CAM: return "CAM";
			case CECD: return "CECD";
			case CFG_U: return "CFG:U";
			case CFG_I: return "CFG:I";
			case CFG_S: return "CFG:S";
			case CFG_NOR: return "CFG:NOR";
			case CSND: return "CSND";
			case DSP: return "DSP";
			case DLP_SRVR: return "DLP::SRVR";
			case DLP_CLNT: return "DLP::CLNT";
			case DLP_FKCL: return "DLP::FKCL";
			case CDC_U: return "CDC:U";
			case GPIO_U: return "GPIO:U";
			case I2C_U: return "I2C:U";
			case MP_U: return "MP:U";
			case PDN_U: return "PDN:U";
			case SPI_U: return "SPI:U";
			case HID: return "HID";
			case HTTP: return "HTTP";
			case IR_USER: return "IR:USER";
			case FRD_A: return "FRD:A";
			case FRD_U: return "FRD:U";
			case FS: return "FS";
			case GPU: return "GSP::GPU";
			case LCD: return "GSP::LCD";
			case LDR_RO: return "LDR:RO";
			case MCU_HWC: return "MCU::HWC";
			case MIC: return "MIC";
			case NDM: return "NDM";
			case NS_C: return "NS:C";
			case NEWS_S: return "NEWS_S";
			case NEWS_U: return "NEWS_U";
			case NWM_EXT: return "nwm::EXT";
			case NWM_CEC: return "nwm::CEC";
			case NWM_INF: return "nwm::INF";
			case NWM_SAP: return "nwm::SAP";
			case NWM_SOC: return "nwm::SOC";
			case NWM_TST: return "nwm::TST";
			case NWM_UDS: return "nwm::UDS";
			case NFC: return "NFC";
			case NIM: return "NIM";
			case PTM_U: return "PTM:U";
			case PTM_SYSM: return "PTM:SYSM";
			case PTM_PLAY: return "PTM:PLAY";
			case PTM_GETS: return "PTM:GETS";
			case PTM_SETS: return "PTM:SETS";
			case SOC: return "SOC";
			case SSL: return "SSL";
			case Y2R: return "Y2R";
			case MVD_STD: return "MVD:STD";
			case PLG_LDR: return "PLG:LDR";
			case PM_APP: return "PM:APP";
			case PM_DBG: return "PM:DBG";
			case PS_PS: return "PS:PS";
			case QTM_C: return "QTM:C";
			case QTM_S: return "QTM:S";
			case QTM_SP: return "QTM:SP";
			case QTM_U: return "QTM:U";
			case ERR_F: return "ERR:F";
			case PXI_DEV: return "PXI:DEV";
			case OS_STUB: return "OS:STUB";
			default: return "Unknown";
		}
	}
}
