#include "../../../include/services/err_f.hpp"

#include <array>

#include "../../../include/ipc.hpp"

namespace ErrFCommands {
	enum : u32 {
		ThrowFatalError = 0x00010800
	};
}

void ErrFService::reset() {}

void ErrFService::throwFatalError(u32 messagePointer) {
	struct ExceptionInfo {
		u8 exceptionType;
		u8 pad[3];
		u32 statusRegister;
		u32 addressRegister;
		u32 fpexc;
		u32 fpinst;
		u32 fpinst2;
	};

	struct ExceptionContext {
		std::array<u32, 16> armRegisters;
		u32 cpsr;
	};

	struct FatalErrorInfoCommon {
		u8 specifier;
		u8 revHigh;
		u16 revLow;
		u32 resultCode;
		u32 pcAddress;
		u32 pid;
		u32 titleIDLow;
		u32 titleIDHigh;
		u32 appTitleIDLow;
		u32 appTitleIDHigh;
	};

	struct FatalErrorInfo {
		FatalErrorInfoCommon common;
		ExceptionInfo exceptionInfo;
		ExceptionContext exceptionContext;
	};

	const u32 dataStart = messagePointer + 4;
	FatalErrorInfo errInfo {};

	auto* raw = reinterpret_cast<u8*>(&errInfo);
	for (u32 i = 0; i < sizeof(FatalErrorInfo); i++) {
		raw[i] = mem.read8(dataStart + i);
	}

	log("ERR:F::ThrowFatalError\n");
	log("  specifier=%u pid=%08X result=%08X pc=%08X\n", errInfo.common.specifier, errInfo.common.pid, errInfo.common.resultCode, errInfo.common.pcAddress);
	log("  titleID=%08X%08X appTitleID=%08X%08X rev=%u.%u\n", errInfo.common.titleIDHigh, errInfo.common.titleIDLow, errInfo.common.appTitleIDHigh,
		errInfo.common.appTitleIDLow, errInfo.common.revHigh, errInfo.common.revLow);

	if (errInfo.common.specifier == 3) {
		log("  exceptionType=%u statusReg=%08X addressReg=%08X cpsr=%08X\n", errInfo.exceptionInfo.exceptionType, errInfo.exceptionInfo.statusRegister,
			errInfo.exceptionInfo.addressRegister, errInfo.exceptionContext.cpsr);
		log("  regs: r0=%08X r1=%08X r2=%08X r3=%08X sp=%08X lr=%08X pc=%08X\n", errInfo.exceptionContext.armRegisters[0], errInfo.exceptionContext.armRegisters[1],
			errInfo.exceptionContext.armRegisters[2], errInfo.exceptionContext.armRegisters[3], errInfo.exceptionContext.armRegisters[13],
			errInfo.exceptionContext.armRegisters[14], errInfo.exceptionContext.armRegisters[15]);
	}

	mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void ErrFService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);

	switch (command) {
		case ErrFCommands::ThrowFatalError: throwFatalError(messagePointer); break;
		default: Helpers::panic("ERR:F service requested. Command: %08X\n", command);
	}
}
