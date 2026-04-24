#include "../../../include/services/gsp_gpu.hpp"

#include "../../../include/PICA/regs.hpp"
#include "../../../include/ipc.hpp"
#include "../../../include/kernel/kernel.hpp"
#include <algorithm>

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
		if (dstStride == 0 || srcStride == 0 || lines == 0) {
			return;
		}

		auto* dst = static_cast<u8*>(mem.getWritePointer(dstVaddr));
		const auto* src = static_cast<const u8*>(mem.getReadPointer(srcVaddr));
		if (dst == nullptr || src == nullptr) {
			Helpers::warn("GSP::GPU framebuffer capture skipped because source/destination pointers are invalid");
			return;
		}

		const u32 bytesPerLine = std::min(dstStride, srcStride);
		for (u32 y = 0; y < lines; y++) {
			std::memcpy(dst, src, bytesPerLine);
			dst += dstStride;
			src += srcStride;
		}
	}

	void clearFramebuffer(Memory& mem, u32 dstVaddr, u32 dstStride, u32 lines) {
		if (dstStride == 0 || lines == 0) {
			return;
		}

		auto* dst = static_cast<u8*>(mem.getWritePointer(dstVaddr));
		if (dst == nullptr) {
			Helpers::warn("GSP::GPU framebuffer clear skipped because destination pointer is invalid");
			return;
		}

		for (u32 y = 0; y < lines; y++) {
			std::memset(dst, 0, dstStride);
			dst += dstStride;
		}
	}
}  // namespace

// Commands used with SendSyncRequest targetted to the GSP::GPU service
namespace ServiceCommands {
	enum : u32 {
		SetAxiConfigQoSMode = 0x00100040,
		ReadHwRegs = 0x00040080,
		AcquireRight = 0x00160042,
		RegisterInterruptRelayQueue = 0x00130042,
		UnregisterInterruptRelayQueue = 0x00140000,
		WriteHwRegs = 0x00010082,
		WriteHwRegsWithMask = 0x00020084,
		SetBufferSwap = 0x00050200,
		FlushDataCache = 0x00080082,
		InvalidateDataCache = 0x00090082,
		SetLCDForceBlack = 0x000B0040,
		TriggerCmdReqQueue = 0x000C0000,
		ReleaseRight = 0x00170000,
		ImportDisplayCaptureInfo = 0x00180000,
		SaveVramSysArea = 0x00190000,
		RestoreVramSysArea = 0x001A0000,
		SetInternalPriorities = 0x001E0080,
		StoreDataCache = 0x001F0082,
	};
}

// Commands written to shared memory and processed by TriggerCmdReqQueue
namespace GXCommands {
	enum : u32 {
		TriggerDMARequest = 0,
		ProcessCommandList = 1,
		MemoryFill = 2,
		TriggerDisplayTransfer = 3,
		TriggerTextureCopy = 4,
		FlushCacheRegions = 5
	};
}

void GPUService::reset() {
	privilegedProcess = 0xFFFFFFFF;  // Set the privileged process to an invalid handle
	interruptEvent = std::nullopt;
	gspThreadCount = 0;
	firstInitialization = true;
	sharedMem = nullptr;
	savedVram.reset();
}

void GPUService::handleSyncRequest(u32 messagePointer) {
	const u32 commandHeader = mem.read32(messagePointer);
	const u32 commandId = commandHeader >> 16;
	switch (commandId) {
		case 0x000C: [[likely]] triggerCmdReqQueue(messagePointer); break;
		case 0x0016: acquireRight(messagePointer); break;
		case 0x0008: flushDataCache(messagePointer); break;
		case 0x0018: importDisplayCaptureInfo(messagePointer); break;
		case 0x0013: registerInterruptRelayQueue(messagePointer); break;
		case 0x0014: unregisterInterruptRelayQueue(messagePointer); break;
		case 0x0017: releaseRight(messagePointer); break;
		case 0x001A: restoreVramSysArea(messagePointer); break;
		case 0x0019: saveVramSysArea(messagePointer); break;
		case 0x0010: setAxiConfigQoSMode(messagePointer); break;
		case 0x0005: setBufferSwap(messagePointer); break;
		case 0x001E: setInternalPriorities(messagePointer); break;
		case 0x000B: setLCDForceBlack(messagePointer); break;
		case 0x001F: storeDataCache(messagePointer); break;
		case 0x0004: readHwRegs(messagePointer); break;
		case 0x0001: writeHwRegs(messagePointer); break;
		case 0x0002: writeHwRegsWithMask(messagePointer); break;
		case 0x0009: invalidateDataCache(messagePointer); break;
		default:
			Helpers::warn("GPU service requested. Command header: %08X (cmd id=%04X)\n", commandHeader, commandId);
			mem.write32(messagePointer, IPC::responseHeader(commandId, 1, 0));
			mem.write32(messagePointer + 4, Result::Success);
			break;
	}
}

void GPUService::acquireRight(u32 messagePointer) {
	const u32 flag = mem.read32(messagePointer + 4);
	const u32 pid = mem.read32(messagePointer + 12);
	log("GSP::GPU::AcquireRight (flag = %X, pid = %X)\n", flag, pid);
	mem.write32(messagePointer, IPC::responseHeader(0x16, 1, 0));

	if (flag != 0) {
		Helpers::warn("GSP::GPU::AcquireRight unsupported flag=%X", flag);
		mem.write32(messagePointer + 4, Result::OS::InvalidCombination);
		return;
	}

	if (pid == KernelHandles::CurrentProcess) {
		privilegedProcess = currentPID;
	} else {
		privilegedProcess = pid;
	}

	mem.write32(messagePointer + 4, Result::Success);
}

void GPUService::releaseRight(u32 messagePointer) {
	log("GSP::GPU::ReleaseRight\n");
	if (privilegedProcess == currentPID) {
		privilegedProcess = 0xFFFFFFFF;
	}

	mem.write32(messagePointer, IPC::responseHeader(0x17, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

// TODO: What is the flags field meant to be?
// What is the "GSP module thread index" meant to be?
// How does the shared memory handle thing work?
void GPUService::registerInterruptRelayQueue(u32 messagePointer) {
	// Detect if this function is called a 2nd time because we'll likely need to impl threads properly for the GSP
	if (gspThreadCount < 0xFF) {
		gspThreadCount += 1;
	}
	const u32 threadIndex = gspThreadCount > 0 ? (gspThreadCount - 1) : 0;

	const u32 flags = mem.read32(messagePointer + 4);
	const u32 eventHandle = mem.read32(messagePointer + 12);
	log("GSP::GPU::RegisterInterruptRelayQueue (flags = %X, event handle = %X)\n", flags, eventHandle);

	const auto event = kernel.getObject(eventHandle, KernelObjectType::Event);
	if (event == nullptr) {  // Check if interrupt event is invalid
		Helpers::warn("Invalid event passed to GSP::GPU::RegisterInterruptRelayQueue");
		mem.write32(messagePointer, IPC::responseHeader(0x13, 1, 0));
		mem.write32(messagePointer + 4, Result::Kernel::InvalidHandle);
		return;
	} else {
		interruptEvent = eventHandle;
	}

	mem.write32(messagePointer, IPC::responseHeader(0x13, 2, 2));
	if (firstInitialization) {
		mem.write32(messagePointer + 4, Result::GSP::SuccessRegisterIRQ);  // First init returns a unique result
		firstInitialization = false;
	} else {
		mem.write32(messagePointer + 4, Result::Success);
	}
	mem.write32(messagePointer + 8, threadIndex);
	mem.write32(messagePointer + 12, 0);                               // Translation descriptor
	mem.write32(messagePointer + 16, KernelHandles::GSPSharedMemHandle);
}

void GPUService::unregisterInterruptRelayQueue(u32 messagePointer) {
	log("GSP::GPU::UnregisterInterruptRelayQueue\n");
	interruptEvent = std::nullopt;
	mem.write32(messagePointer, IPC::responseHeader(0x14, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void GPUService::requestInterrupt(GPUInterrupt type) {
	if (sharedMem == nullptr) [[unlikely]] {  // Shared memory hasn't been set up yet
		return;
	}

	// TODO: Add support for multiple GSP threads
	u8 index = sharedMem[0];  // The interrupt block is normally located at sharedMem + processGSPIndex*0x40
	u8& interruptCount = sharedMem[1];
	u8 flagIndex = (index + interruptCount) % 0x34;
	interruptCount++;

	sharedMem[2] = 0;                                    // Set error code to 0
	sharedMem[0xC + flagIndex] = static_cast<u8>(type);  // Write interrupt type to queue

	// Update framebuffer info in shared memory
	// Most new games check to make sure that the "flag" byte of the framebuffer info header is set to 0
	// Not emulating this causes Yoshi's Wooly World, Captain Toad, Metroid 2 et al to hang
	if (type == GPUInterrupt::VBlank0 || type == GPUInterrupt::VBlank1) {
		int screen = static_cast<u32>(type) - static_cast<u32>(GPUInterrupt::VBlank0);  // 0 for top screen, 1 for bottom
		const u32 threadCount = 4;
		for (u32 thread = 0; thread < threadCount; ++thread) {
			FramebufferUpdate* update = getFramebufferInfo(thread, screen);
			if (update->dirtyFlag & 1) {
				setBufferSwapImpl(screen, update->framebufferInfo[update->index]);
				update->dirtyFlag &= ~1;
			}
		}
	}

	// Signal interrupt event
	if (interruptEvent.has_value()) {
		kernel.signalEvent(interruptEvent.value());
	}
}

void GPUService::readHwRegs(u32 messagePointer) {
	u32 ioAddr = mem.read32(messagePointer + 4);      // GPU address based at 0x1EB00000, word aligned
	const u32 size = mem.read32(messagePointer + 8);  // Size in bytes
	const u32 initialDataPointer = mem.read32(messagePointer + 0x104);
	u32 dataPointer = initialDataPointer;
	log("GSP::GPU::ReadHwRegs (GPU address = %08X, size = %X, data address = %08X)\n", ioAddr, size, dataPointer);

	// Check for alignment
	if ((size & 3) || (ioAddr & 3) || (dataPointer & 3)) {
		mem.write32(messagePointer, IPC::responseHeader(0x4, 1, 0));
		mem.write32(messagePointer + 4, Result::OS::InvalidCombination);
		return;
	}

	if (size > 0x80) {
		mem.write32(messagePointer, IPC::responseHeader(0x4, 1, 0));
		mem.write32(messagePointer + 4, Result::OS::InvalidCombination);
		return;
	}

	if (ioAddr >= 0x420000) {
		mem.write32(messagePointer, IPC::responseHeader(0x4, 1, 0));
		mem.write32(messagePointer + 4, Result::OS::InvalidCombination);
		return;
	}

	ioAddr += 0x1EB00000;
	// Read the PICA registers and write them to the output buffer
	for (u32 i = 0; i < size; i += 4) {
		const u32 value = gpu.readReg(ioAddr);
		mem.write32(dataPointer, value);
		dataPointer += 4;
		ioAddr += 4;
	}

	mem.write32(messagePointer, IPC::responseHeader(0x4, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
	// Translation descriptor. TODO: Make a more generic interface for this
	mem.write32(messagePointer + 8, u32(size << 14) | 2);
	mem.write32(messagePointer + 12, initialDataPointer);
}

void GPUService::writeHwRegs(u32 messagePointer) {
	u32 ioAddr = mem.read32(messagePointer + 4);      // GPU address based at 0x1EB00000, word aligned
	const u32 size = mem.read32(messagePointer + 8);  // Size in bytes
	u32 dataPointer = mem.read32(messagePointer + 16);
	log("GSP::GPU::writeHwRegs (GPU address = %08X, size = %X, data address = %08X)\n", ioAddr, size, dataPointer);

	// Check for alignment
	if ((size & 3) || (ioAddr & 3) || (dataPointer & 3)) {
		mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 0));
		mem.write32(messagePointer + 4, Result::OS::InvalidCombination);
		return;
	}

	if (size > 0x80) {
		mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 0));
		mem.write32(messagePointer + 4, Result::OS::InvalidCombination);
		return;
	}

	if (ioAddr >= 0x420000) {
		mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 0));
		mem.write32(messagePointer + 4, Result::OS::InvalidCombination);
		return;
	}

	ioAddr += 0x1EB00000;
	for (u32 i = 0; i < size; i += 4) {
		const u32 value = mem.read32(dataPointer);
		gpu.writeReg(ioAddr, value);
		dataPointer += 4;
		ioAddr += 4;
	}

	mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

// Update sequential GPU registers using an array of data and mask values using this formula
// GPU register = (register & ~mask) | (data & mask).
void GPUService::writeHwRegsWithMask(u32 messagePointer) {
	u32 ioAddr = mem.read32(messagePointer + 4);      // GPU address based at 0x1EB00000, word aligned
	const u32 size = mem.read32(messagePointer + 8);  // Size in bytes

	u32 dataPointer = mem.read32(messagePointer + 16);  // Data pointer
	u32 maskPointer = mem.read32(messagePointer + 24);  // Mask pointer

	log("GSP::GPU::writeHwRegsWithMask (GPU address = %08X, size = %X, data address = %08X, mask address = %08X)\n", ioAddr, size, dataPointer,
		maskPointer);

	// Check for alignment
	if ((size & 3) || (ioAddr & 3) || (dataPointer & 3) || (maskPointer & 3)) {
		mem.write32(messagePointer, IPC::responseHeader(0x2, 1, 0));
		mem.write32(messagePointer + 4, Result::OS::InvalidCombination);
		return;
	}

	if (size > 0x80) {
		mem.write32(messagePointer, IPC::responseHeader(0x2, 1, 0));
		mem.write32(messagePointer + 4, Result::OS::InvalidCombination);
		return;
	}

	if (ioAddr >= 0x420000) {
		mem.write32(messagePointer, IPC::responseHeader(0x2, 1, 0));
		mem.write32(messagePointer + 4, Result::OS::InvalidCombination);
		return;
	}

	ioAddr += 0x1EB00000;
	for (u32 i = 0; i < size; i += 4) {
		const u32 current = gpu.readReg(ioAddr);
		const u32 data = mem.read32(dataPointer);
		const u32 mask = mem.read32(maskPointer);

		u32 newValue = (current & ~mask) | (data & mask);

		gpu.writeReg(ioAddr, newValue);
		maskPointer += 4;
		dataPointer += 4;
		ioAddr += 4;
	}

	mem.write32(messagePointer, IPC::responseHeader(0x2, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void GPUService::flushDataCache(u32 messagePointer) {
	u32 address = mem.read32(messagePointer + 4);
	u32 size = mem.read32(messagePointer + 8);
	u32 processHandle = mem.read32(messagePointer + 16);
	log("GSP::GPU::FlushDataCache(address = %08X, size = %X, process = %X)\n", address, size, processHandle);

	mem.write32(messagePointer, IPC::responseHeader(0x8, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void GPUService::invalidateDataCache(u32 messagePointer) {
	u32 address = mem.read32(messagePointer + 4);
	u32 size = mem.read32(messagePointer + 8);
	u32 processHandle = mem.read32(messagePointer + 16);
	log("GSP::GPU::InvalidateDataCache(address = %08X, size = %X, process = %X)\n", address, size, processHandle);

	mem.write32(messagePointer, IPC::responseHeader(0x9, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void GPUService::storeDataCache(u32 messagePointer) {
	u32 address = mem.read32(messagePointer + 4);
	u32 size = mem.read32(messagePointer + 8);
	u32 processHandle = mem.read32(messagePointer + 16);
	log("GSP::GPU::StoreDataCache(address = %08X, size = %X, process = %X)\n", address, size, processHandle);

	mem.write32(messagePointer, IPC::responseHeader(0x1F, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void GPUService::setLCDForceBlack(u32 messagePointer) {
	u32 flag = mem.read32(messagePointer + 4);
	log("GSP::GPU::SetLCDForceBlank(flag = %d)\n", flag);

	if (flag != 0) {
		printf("Filled both LCDs with black\n");
	}

	mem.write32(messagePointer, IPC::responseHeader(0xB, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void GPUService::triggerCmdReqQueue(u32 messagePointer) {
	processCommandBuffer();
	// Many titles rely on shared-memory framebuffer updates becoming visible quickly.
	// On hardware this is synchronized with VBlank, but our current interrupt/timing
	// model does not always deliver VBlank-driven swaps soon enough.
	if (sharedMem != nullptr) {
		const u32 threadCount = 4;
		for (u32 thread = 0; thread < threadCount; ++thread) {
			for (int screen = 0; screen < 2; ++screen) {
				FramebufferUpdate* update = getFramebufferInfo(thread, screen);
				if ((update->dirtyFlag & 1) != 0) {
					setBufferSwapImpl(screen, update->framebufferInfo[update->index]);
					update->dirtyFlag &= ~1;
				}
			}
		}
	}
	mem.write32(messagePointer, IPC::responseHeader(0xC, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

// Seems to be completely undocumented, probably not very important or useful
void GPUService::setAxiConfigQoSMode(u32 messagePointer) {
	log("GSP::GPU::SetAxiConfigQoSMode\n");
	mem.write32(messagePointer, IPC::responseHeader(0x10, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void GPUService::setBufferSwap(u32 messagePointer) {
	FramebufferInfo info{};
	const u32 screenId = mem.read32(messagePointer + 4);  // Selects either PDC0 or PDC1
	info.activeFb = mem.read32(messagePointer + 8);
	info.leftFramebufferVaddr = mem.read32(messagePointer + 12);
	info.rightFramebufferVaddr = mem.read32(messagePointer + 16);
	info.stride = mem.read32(messagePointer + 20);
	info.format = mem.read32(messagePointer + 24);
	info.displayFb = mem.read32(messagePointer + 28);  // Selects either framebuffer A or B

	log("GSP::GPU::SetBufferSwap\n");
	Helpers::warn("Untested GSP::GPU::SetBufferSwap call");

	setBufferSwapImpl(screenId, info);
	mem.write32(messagePointer, IPC::responseHeader(0x05, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

// Seems to also be completely undocumented
void GPUService::setInternalPriorities(u32 messagePointer) {
	log("GSP::GPU::SetInternalPriorities\n");
	mem.write32(messagePointer, IPC::responseHeader(0x1E, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void GPUService::processCommandBuffer() {
	if (sharedMem == nullptr) [[unlikely]] {  // Shared memory hasn't been set up yet
		return;
	}

	constexpr int threadCount = 4;
	for (int t = 0; t < threadCount; t++) {
		u8* cmdBuffer = &sharedMem[0x800 + t * 0x200];
		u8& index = cmdBuffer[0];
		u8& commandsLeft = cmdBuffer[1];
		// Commands start at byte 0x20 of the command buffer, each being 0x20 bytes long

		log("Processing %d GPU commands\n", commandsLeft);

		while (commandsLeft != 0) {
			u32* cmd = reinterpret_cast<u32*>(&cmdBuffer[0x20 + ((index & 0xF) * 0x20)]);
			const u32 cmdID = cmd[0] & 0xff;
			switch (cmdID) {
				case GXCommands::ProcessCommandList: processCommandList(cmd); break;
				case GXCommands::MemoryFill: memoryFill(cmd); break;
				case GXCommands::TriggerDisplayTransfer: triggerDisplayTransfer(cmd); break;
				case GXCommands::TriggerDMARequest: triggerDMARequest(cmd); break;
				case GXCommands::TriggerTextureCopy: triggerTextureCopy(cmd); break;
				case GXCommands::FlushCacheRegions: flushCacheRegions(cmd); break;
				default: Helpers::warn("GSP::GPU::ProcessCommands: Unknown cmd ID %d", cmdID); break;
			}

			index = (index + 1) % 15;
			commandsLeft--;
		}
	}
}

static u32 VaddrToPaddr(u32 addr) {
	if (addr >= VirtualAddrs::VramStart && addr < (VirtualAddrs::VramStart + VirtualAddrs::VramSize)) [[likely]] {
		return addr - VirtualAddrs::VramStart + PhysicalAddrs::VRAM;
	}

	else if (addr >= VirtualAddrs::LinearHeapStartOld && addr < VirtualAddrs::LinearHeapEndOld) {
		return addr - VirtualAddrs::LinearHeapStartOld + PhysicalAddrs::FCRAM;
	}

	else if (addr >= VirtualAddrs::LinearHeapStartNew && addr < VirtualAddrs::LinearHeapEndNew) {
		return addr - VirtualAddrs::LinearHeapStartNew + PhysicalAddrs::FCRAM;
	}

	else if (addr == 0) {
		return 0;
	}

	Helpers::warn("[GSP::GPU VaddrToPaddr] Unknown virtual address %08X", addr);
	// Return 0 so downstream framebuffer routing can fall back cleanly instead of
	// carrying a non-null garbage address that can mask diagnostics.
	return 0;
}

// Fill 2 GPU framebuffers, buf0 and buf1, using a specific word value
void GPUService::memoryFill(u32* cmd) {
	u32 control = cmd[7];

	// buf0 parameters
	u32 start0 = cmd[1];  // Start address for the fill. If 0, don't fill anything
	u32 value0 = cmd[2];  // Value to fill the framebuffer with
	u32 end0 = cmd[3];    // End address for the fill
	u32 control0 = control & 0xffff;

	// buf1 parameters
	u32 start1 = cmd[4];
	u32 value1 = cmd[5];
	u32 end1 = cmd[6];
	u32 control1 = control >> 16;

	if (start0 != 0) {
		gpu.clearBuffer(VaddrToPaddr(start0), VaddrToPaddr(end0), value0, control0);
		requestInterrupt(GPUInterrupt::PSC0);
	}

	if (start1 != 0) {
		gpu.clearBuffer(VaddrToPaddr(start1), VaddrToPaddr(end1), value1, control1);
		requestInterrupt(GPUInterrupt::PSC1);
	}
}

void GPUService::triggerDisplayTransfer(u32* cmd) {
	const u32 inputAddr = VaddrToPaddr(cmd[1]);
	const u32 outputAddr = VaddrToPaddr(cmd[2]);
	const u32 inputSize = cmd[3];
	const u32 outputSize = cmd[4];
	const u32 flags = cmd[5];

	log("GSP::GPU::TriggerDisplayTransfer (Stubbed)\n");
	gpu.displayTransfer(inputAddr, outputAddr, inputSize, outputSize, flags);
	requestInterrupt(GPUInterrupt::PPF);  // Send "Display transfer finished" interrupt
}

void GPUService::triggerDMARequest(u32* cmd) {
	const u32 source = cmd[1];
	const u32 dest = cmd[2];
	const u32 size = cmd[3];
	const bool flush = cmd[7] == 1;

	log("GSP::GPU::TriggerDMARequest (source = %08X, dest = %08X, size = %08X)\n", source, dest, size);
	gpu.fireDMA(dest, source, size);
	requestInterrupt(GPUInterrupt::DMA);
}

void GPUService::flushCacheRegions(u32* cmd) { log("GSP::GPU::FlushCacheRegions (Stubbed)\n"); }

void GPUService::setBufferSwapImpl(u32 screenId, const FramebufferInfo& info) {
	using namespace PICA::ExternalRegs;
	if (screenId > 1) {
		Helpers::warn("GSP::GPU::SetBufferSwapImpl got invalid screenId=%u", screenId);
		return;
	}
	if ((info.format & 0x7) > static_cast<u32>(PICA::ColorFmt::RGBA4)) {
		Helpers::warn("GSP::GPU::SetBufferSwapImpl got unsupported color format=%u for screenId=%u", info.format, screenId);
	}

	static constexpr std::array<u32, 8> fbAddresses = {
		Framebuffer0AFirstAddr, Framebuffer0ASecondAddr, Framebuffer0BFirstAddr, Framebuffer0BSecondAddr,
		Framebuffer1AFirstAddr, Framebuffer1ASecondAddr, Framebuffer1BFirstAddr, Framebuffer1BSecondAddr,
	};

	auto& regs = gpu.getExtRegisters();

	const u32 fbPair = screenId * 2 + (info.activeFb & 1);
	const u32 fbIndex = fbPair * 2;
	regs[fbAddresses[fbIndex]] = VaddrToPaddr(info.leftFramebufferVaddr);
	regs[fbAddresses[fbIndex + 1]] = VaddrToPaddr(info.rightFramebufferVaddr);

	static constexpr std::array<u32, 6> configAddresses = {
		Framebuffer0Config, Framebuffer0Select, Framebuffer0Stride, Framebuffer1Config, Framebuffer1Select, Framebuffer1Stride,
	};

	const u32 configIndex = screenId * 3;
	regs[configAddresses[configIndex]] = info.format;
	regs[configAddresses[configIndex + 1]] = info.displayFb;
	regs[configAddresses[configIndex + 2]] = info.stride;
}

// Actually send command list (aka display list) to GPU
void GPUService::processCommandList(u32* cmd) {
	const u32 address = cmd[1] & ~7;                        // Buffer address
	const u32 size = cmd[2] & ~3;                           // Buffer size in bytes
	[[maybe_unused]] const bool updateGas = cmd[3] == 1;    // Update gas additive blend results (0 = don't update, 1 = update)
	[[maybe_unused]] const bool flushBuffer = cmd[7] == 1;  // Flush buffer (0 = don't flush, 1 = flush)

	log("GPU::GSP::processCommandList. Address: %08X, size in bytes: %08X\n", address, size);
	gpu.startCommandList(address, size);
	requestInterrupt(GPUInterrupt::P3D);  // Send an IRQ when command list processing is over
}

// TODO: Emulate the transfer engine & its registers
// Then this can be emulated by just writing the appropriate values there
void GPUService::triggerTextureCopy(u32* cmd) {
	const u32 inputAddr = VaddrToPaddr(cmd[1]);
	const u32 outputAddr = VaddrToPaddr(cmd[2]);
	const u32 totalBytes = cmd[3];
	const u32 inputSize = cmd[4];
	const u32 outputSize = cmd[5];
	const u32 flags = cmd[6];

	log("GSP::GPU::TriggerTextureCopy (Stubbed)\n");
	gpu.textureCopy(inputAddr, outputAddr, totalBytes, inputSize, outputSize, flags);
	// This uses the transfer engine and thus needs to fire a PPF interrupt.
	// NSMB2 relies on this
	requestInterrupt(GPUInterrupt::PPF);
}

// Used when transitioning from the app to an OS applet, such as software keyboard, mii maker, mii selector, etc
// Stubbed until we decide to support LLE applets
void GPUService::saveVramSysArea(u32 messagePointer) {
	log("GSP::GPU::SaveVramSysArea\n");
	auto* vram = static_cast<const u8*>(mem.getReadPointer(VirtualAddrs::VramStart));
	if (vram != nullptr) {
		auto& storage = savedVram.emplace(VirtualAddrs::VramSize);
		std::memcpy(storage.data(), vram, storage.size());
	}

	if (sharedMem != nullptr) {
		auto* topScreen = getTopFramebufferInfo(0);
		auto* bottomScreen = getBottomFramebufferInfo(0);

		auto captureScreen = [&](FramebufferUpdate* update, int screenId, u32 leftSaveArea, u32 rightSaveArea, u32 lines, bool hasRightBuffer) {
			const auto& current = update->framebufferInfo[update->index];
			const u32 formatIndex = current.format & 7;
			const u32 bytesPerPixel = BytesPerPixelByFormat[formatIndex];
			const u32 packedStride = FramebufferWidth * bytesPerPixel;
			if (bytesPerPixel == 0 || packedStride == 0) {
				clearFramebuffer(mem, leftSaveArea, packedStride, lines);
				if (hasRightBuffer) {
					clearFramebuffer(mem, rightSaveArea, packedStride, lines);
				}
				return;
			}

			if (current.leftFramebufferVaddr != 0) {
				copyFramebuffer(mem, leftSaveArea, current.leftFramebufferVaddr, packedStride, current.stride, lines);
			} else {
				clearFramebuffer(mem, leftSaveArea, packedStride, lines);
			}

			if (hasRightBuffer) {
				if (current.rightFramebufferVaddr != 0) {
					copyFramebuffer(mem, rightSaveArea, current.rightFramebufferVaddr, packedStride, current.stride, lines);
				} else {
					clearFramebuffer(mem, rightSaveArea, packedStride, lines);
				}
			}

			auto savedInfo = current;
			savedInfo.leftFramebufferVaddr = leftSaveArea;
			if (hasRightBuffer) {
				savedInfo.rightFramebufferVaddr = rightSaveArea;
			}
			savedInfo.stride = packedStride;
			setBufferSwapImpl(screenId, savedInfo);
		};

		captureScreen(topScreen, 0, FramebufferSaveAreaTopLeft, FramebufferSaveAreaTopRight, TopFramebufferHeight, true);
		captureScreen(bottomScreen, 1, FramebufferSaveAreaBottom, 0, BottomFramebufferHeight, false);
	}

	mem.write32(messagePointer, IPC::responseHeader(0x19, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void GPUService::restoreVramSysArea(u32 messagePointer) {
	log("GSP::GPU::RestoreVramSysArea\n");
	if (savedVram.has_value()) {
		auto* vram = static_cast<u8*>(mem.getWritePointer(VirtualAddrs::VramStart));
		if (vram != nullptr) {
			const auto& storage = savedVram.value();
			std::memcpy(vram, storage.data(), storage.size());
		}
	}

	if (sharedMem != nullptr) {
		auto* topScreen = getTopFramebufferInfo(0);
		auto* bottomScreen = getBottomFramebufferInfo(0);
		setBufferSwapImpl(0, topScreen->framebufferInfo[topScreen->index]);
		setBufferSwapImpl(1, bottomScreen->framebufferInfo[bottomScreen->index]);
	}

	mem.write32(messagePointer, IPC::responseHeader(0x1A, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

// Used in similar fashion to the SaveVramSysArea function
void GPUService::importDisplayCaptureInfo(u32 messagePointer) {
	log("GSP::GPU::ImportDisplayCaptureInfo\n");

	mem.write32(messagePointer, IPC::responseHeader(0x18, 9, 0));
	mem.write32(messagePointer + 4, Result::Success);

	if (sharedMem == nullptr) {
		Helpers::warn("GSP::GPU::ImportDisplayCaptureInfo called without GSP module being properly initialized!");
		return;
	}

	FramebufferUpdate* topScreen = getTopFramebufferInfo(0);
	FramebufferUpdate* bottomScreen = getBottomFramebufferInfo(0);

	// Capture the relevant data for both screens and return them to the caller
	CaptureInfo topScreenCapture = {
		.leftFramebuffer = topScreen->framebufferInfo[topScreen->index].leftFramebufferVaddr,
		.rightFramebuffer = topScreen->framebufferInfo[topScreen->index].rightFramebufferVaddr,
		.format = topScreen->framebufferInfo[topScreen->index].format,
		.stride = topScreen->framebufferInfo[topScreen->index].stride,
	};

	CaptureInfo bottomScreenCapture = {
		.leftFramebuffer = bottomScreen->framebufferInfo[bottomScreen->index].leftFramebufferVaddr,
		.rightFramebuffer = bottomScreen->framebufferInfo[bottomScreen->index].rightFramebufferVaddr,
		.format = bottomScreen->framebufferInfo[bottomScreen->index].format,
		.stride = bottomScreen->framebufferInfo[bottomScreen->index].stride,
	};

	mem.write32(messagePointer + 8, topScreenCapture.leftFramebuffer);
	mem.write32(messagePointer + 12, topScreenCapture.rightFramebuffer);
	mem.write32(messagePointer + 16, topScreenCapture.format);
	mem.write32(messagePointer + 20, topScreenCapture.stride);

	mem.write32(messagePointer + 24, bottomScreenCapture.leftFramebuffer);
	mem.write32(messagePointer + 28, bottomScreenCapture.rightFramebuffer);
	mem.write32(messagePointer + 32, bottomScreenCapture.format);
	mem.write32(messagePointer + 36, bottomScreenCapture.stride);
}
