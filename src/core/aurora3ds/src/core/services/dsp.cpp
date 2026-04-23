#include "../../../include/services/dsp.hpp"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include "../../../include/apple_crypto.hpp"
#include "../../../include/config.hpp"
#include "../../../include/ipc.hpp"
#include "../../../include/kernel/kernel.hpp"
#include "../../../include/services/dsp_firmware_db.hpp"

namespace DSPCommands {
	enum : u32 {
		RecvData = 0x00010040,
		RecvDataIsReady = 0x00020040,
		SendData = 0x00030040,
		SendDataIsEmpty = 0x00040040,
		SendFifoEx = 0x000500C2,
		RecvFifoEx = 0x000600C2,
		SetSemaphore = 0x00070040,
		GetSemaphore = 0x00080000,
		ClearSemaphore = 0x00090040,
		MaskSemaphore = 0x000A0040,
		CheckSemaphoreRequest = 0x000B0000,
		ConvertProcessAddressFromDspDram = 0x000C0040,
		WriteProcessPipe = 0x000D0082,
		ReadPipe = 0x000E00C2,
		GetPipeReadableSize = 0x000F0040,
		ReadPipeIfPossible = 0x001000C0,
		LoadComponent = 0x001100C2,
		UnloadComponent = 0x00120000,
		FlushDataCache = 0x00130082,
		InvalidateDataCache = 0x00140082,
		RegisterInterruptEvents = 0x00150082,
		GetSemaphoreEventHandle = 0x00160000,
		SetSemaphoreMask = 0x00170040,
		GetPhysicalAddress = 0x00180040,
		GetVirtualAddress = 0x00190040,
		GetHeadphoneStatus = 0x001F0000,
		ForceHeadphoneOut = 0x00200040,
		GetIsDspOccupied = 0x00210000,
	};
}

void DSPService::reset() {
	totalEventCount = 0;
	semaphoreMask = 0;
	headphonesInserted = true;
	semaphoreEvent = std::nullopt;
	interrupt0 = std::nullopt;
	interrupt1 = std::nullopt;
}

void DSPService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	IPC::RequestParser rp(messagePointer, mem);

	switch (command) {
		case DSPCommands::GetSemaphoreEventHandle: {
			log("DSP::GetSemaphoreEventHandle\n");
			if (!semaphoreEvent) semaphoreEvent = kernel.makeEvent(ResetType::OneShot);
			auto rb = rp.MakeBuilder(1, 2);
			rb.Push(Result::Success);
			rb.Push(IPC::CopyHandleDesc(1));
			rb.Push(semaphoreEvent.value());
			break;
		}
		case DSPCommands::RegisterInterruptEvents: {
			log("DSP::RegisterInterruptEvents\n");
			auto rb = rp.MakeBuilder(1, 0);
			rb.Push(Result::Success);
			break;
		}
		case DSPCommands::LoadComponent: {
			log("DSP::LoadComponent\n");
			auto rb = rp.MakeBuilder(2, 0);
			rb.Push(Result::Success);
			rb.Push(1u); // Component size placeholder
			break;
		}
		case DSPCommands::GetHeadphoneStatus: {
			log("DSP::GetHeadphoneStatus\n");
			auto rb = rp.MakeBuilder(2, 0);
			rb.Push(Result::Success);
			rb.Push(headphonesInserted ? 1u : 0u);
			break;
		}
		default:
			log("DSP service requested unknown command: %08X\n", command);
			auto rb = rp.MakeBuilder(1, 0);
			rb.Push(Result::Success);
			break;
	}
}
