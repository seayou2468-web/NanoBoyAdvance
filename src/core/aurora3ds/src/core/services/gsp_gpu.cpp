#include "../../../include/services/gsp_gpu.hpp"
#include "../../../include/PICA/regs.hpp"
#include "../../../include/ipc.hpp"
#include "../../../include/kernel/kernel.hpp"

namespace {
	constexpr u32 FramebufferWidth = 240;
	constexpr u32 TopFramebufferHeight = 400;
	constexpr u32 BottomFramebufferHeight = 320;

	constexpr u32 FramebufferSaveAreaTopLeft = VirtualAddrs::VramStart;
	constexpr u32 FramebufferSaveAreaTopRight = FramebufferSaveAreaTopLeft + FramebufferWidth * TopFramebufferHeight * 3;
	constexpr u32 FramebufferSaveAreaBottom = FramebufferSaveAreaTopRight + FramebufferWidth * TopFramebufferHeight * 3;

	constexpr std::array<u8, 8> BytesPerPixelByFormat = {
		4, 3, 2, 2, 2, 0, 0, 0,
	};

	void copyFramebuffer(Memory& mem, u32 dstVaddr, u32 srcVaddr, u32 dstStride, u32 srcStride, u32 lines) {
		if (dstStride == 0 || srcStride == 0 || lines == 0) return;
		auto* dst = static_cast<u8*>(mem.getWritePointer(dstVaddr));
		const auto* src = static_cast<const u8*>(mem.getReadPointer(srcVaddr));
		if (dst == nullptr || src == nullptr) return;
		const u32 bytesPerLine = std::min(dstStride, srcStride);
		for (u32 y = 0; y < lines; y++) {
			std::memcpy(dst, src, bytesPerLine);
			dst += dstStride;
			src += srcStride;
		}
	}
}

namespace ServiceCommands {
	enum : u32 {
		WriteHwRegs = 0x00010082,
		WriteHwRegsWithMask = 0x00020084,
		WriteHwRegRepeat = 0x000300C2,
		ReadHwRegs = 0x00040080,
		SetBufferSwap = 0x00050200,
		SetCommandList = 0x00060082,
		RequestDma = 0x00070082,
		FlushDataCache = 0x00080082,
		InvalidateDataCache = 0x00090082,
		RegisterInterruptEvents = 0x000A0000,
		SetLCDForceBlack = 0x000B0040,
		TriggerCmdReqQueue = 0x000C0000,
		SetDisplayTransfer = 0x000D0140,
		SetTextureCopy = 0x000E0180,
		SetMemoryFill = 0x000F0180,
		SetAxiConfigQoSMode = 0x00100040,
		RegisterInterruptRelayQueue = 0x00130042,
		UnregisterInterruptRelayQueue = 0x00140000,
		TryAcquireRight = 0x00150000,
		AcquireRight = 0x00160042,
		ReleaseRight = 0x00170000,
		ImportDisplayCaptureInfo = 0x00180000,
		SaveVramSysArea = 0x00190000,
		RestoreVramSysArea = 0x001A0000,
		SetInternalPriorities = 0x001E0080,
		StoreDataCache = 0x001F0082,
	};
}

void GPUService::reset() {
	privilegedProcess = 0xFFFFFFFF;
	interruptEvent = std::nullopt;
	gspThreadCount = 0;
	firstInitialization = true;
	sharedMem = nullptr;
	savedVram.reset();
}

void GPUService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	IPC::RequestParser rp(messagePointer, mem);

	switch (command) {
		case ServiceCommands::WriteHwRegs: writeHwRegs(messagePointer); break;
		case ServiceCommands::WriteHwRegsWithMask: writeHwRegsWithMask(messagePointer); break;
		case ServiceCommands::ReadHwRegs: readHwRegs(messagePointer); break;
		case ServiceCommands::SetBufferSwap: setBufferSwap(messagePointer); break;
		case ServiceCommands::FlushDataCache: flushDataCache(messagePointer); break;
		case ServiceCommands::InvalidateDataCache: invalidateDataCache(messagePointer); break;
		case ServiceCommands::SetLCDForceBlack: setLCDForceBlack(messagePointer); break;
		case ServiceCommands::TriggerCmdReqQueue: triggerCmdReqQueue(messagePointer); break;
		case ServiceCommands::SetAxiConfigQoSMode: setAxiConfigQoSMode(messagePointer); break;
		case ServiceCommands::RegisterInterruptRelayQueue: registerInterruptRelayQueue(messagePointer); break;
		case ServiceCommands::UnregisterInterruptRelayQueue: unregisterInterruptRelayQueue(messagePointer); break;
		case ServiceCommands::AcquireRight: acquireRight(messagePointer); break;
		case ServiceCommands::ReleaseRight: releaseRight(messagePointer); break;
		case ServiceCommands::ImportDisplayCaptureInfo: importDisplayCaptureInfo(messagePointer); break;
		case ServiceCommands::SaveVramSysArea: saveVramSysArea(messagePointer); break;
		case ServiceCommands::RestoreVramSysArea: restoreVramSysArea(messagePointer); break;
		case ServiceCommands::SetInternalPriorities: setInternalPriorities(messagePointer); break;
		case ServiceCommands::StoreDataCache: storeDataCache(messagePointer); break;
		case ServiceCommands::TryAcquireRight: {
			log("GSP::GPU::TryAcquireRight\n");
			auto rb = rp.MakeBuilder(2, 0);
			rb.Push(Result::Success);
			rb.Push(1u); // Success
			break;
		}
		default:
			log("GSP::GPU unknown command: %08X\n", command);
			auto rb = rp.MakeBuilder(1, 0);
			rb.Push(Result::Success);
			break;
	}
}

void GPUService::writeHwRegs(u32 messagePointer) {
	log("GSP::GPU::WriteHwRegs\n");
	mem.write32(messagePointer + 4, Result::Success);
}

void GPUService::readHwRegs(u32 messagePointer) {
	log("GSP::GPU::ReadHwRegs\n");
	mem.write32(messagePointer + 4, Result::Success);
}

void GPUService::writeHwRegsWithMask(u32 messagePointer) {
	log("GSP::GPU::WriteHwRegsWithMask\n");
	mem.write32(messagePointer + 4, Result::Success);
}

void GPUService::setBufferSwap(u32 messagePointer) {
	log("GSP::GPU::SetBufferSwap\n");
	mem.write32(messagePointer + 4, Result::Success);
}

void GPUService::flushDataCache(u32 messagePointer) {
	log("GSP::GPU::FlushDataCache\n");
	mem.write32(messagePointer + 4, Result::Success);
}

void GPUService::invalidateDataCache(u32 messagePointer) {
	log("GSP::GPU::InvalidateDataCache\n");
	mem.write32(messagePointer + 4, Result::Success);
}

void GPUService::setLCDForceBlack(u32 messagePointer) {
	log("GSP::GPU::SetLCDForceBlack\n");
	mem.write32(messagePointer + 4, Result::Success);
}

void GPUService::triggerCmdReqQueue(u32 messagePointer) {
	log("GSP::GPU::TriggerCmdReqQueue\n");
	mem.write32(messagePointer + 4, Result::Success);
}

void GPUService::setAxiConfigQoSMode(u32 messagePointer) {
	log("GSP::GPU::SetAxiConfigQoSMode\n");
	mem.write32(messagePointer + 4, Result::Success);
}

void GPUService::registerInterruptRelayQueue(u32 messagePointer) {
	log("GSP::GPU::RegisterInterruptRelayQueue\n");
	mem.write32(messagePointer + 4, Result::Success);
}

void GPUService::unregisterInterruptRelayQueue(u32 messagePointer) {
	log("GSP::GPU::UnregisterInterruptRelayQueue\n");
	mem.write32(messagePointer + 4, Result::Success);
}

void GPUService::acquireRight(u32 messagePointer) {
	log("GSP::GPU::AcquireRight\n");
	mem.write32(messagePointer + 4, Result::Success);
}

void GPUService::releaseRight(u32 messagePointer) {
	log("GSP::GPU::ReleaseRight\n");
	mem.write32(messagePointer + 4, Result::Success);
}

void GPUService::importDisplayCaptureInfo(u32 messagePointer) {
	log("GSP::GPU::ImportDisplayCaptureInfo\n");
	mem.write32(messagePointer + 4, Result::Success);
}

void GPUService::saveVramSysArea(u32 messagePointer) {
	log("GSP::GPU::SaveVramSysArea\n");
	mem.write32(messagePointer + 4, Result::Success);
}

void GPUService::restoreVramSysArea(u32 messagePointer) {
	log("GSP::GPU::RestoreVramSysArea\n");
	mem.write32(messagePointer + 4, Result::Success);
}

void GPUService::setInternalPriorities(u32 messagePointer) {
	log("GSP::GPU::SetInternalPriorities\n");
	mem.write32(messagePointer + 4, Result::Success);
}

void GPUService::storeDataCache(u32 messagePointer) {
	log("GSP::GPU::StoreDataCache\n");
	mem.write32(messagePointer + 4, Result::Success);
}

void GPUService::requestInterrupt(GPUInterrupt type) {
	// TODO: Handle interrupts
}
