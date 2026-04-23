#include "../../../include/services/y2r.hpp"
#include <algorithm>
#include "../../../include/ipc.hpp"
#include "../../../include/kernel/kernel.hpp"

namespace Y2RCommands {
	enum : u32 {
		SetInputFormat = 0x00010040,
		SetOutputFormat = 0x00030040,
		GetOutputFormat = 0x00040000,
		SetRotation = 0x00050040,
		SetBlockAlignment = 0x00070040,
		GetBlockAlignment = 0x00080000,
		SetSpacialDithering = 0x00090040,
		SetTemporalDithering = 0x000B0040,
		SetTransferEndInterrupt = 0x000D0040,
		GetTransferEndEvent = 0x000F0000,
		SetSendingY = 0x00100102,
		SetSendingU = 0x00110102,
		SetSendingV = 0x00120102,
		SetSendingYUV = 0x00130102,
		IsFinishedSendingYUV = 0x00140000,
		IsFinishedSendingY = 0x00150000,
		IsFinishedSendingU = 0x00160000,
		IsFinishedSendingV = 0x00170000,
		SetReceiving = 0x00180102,
		IsFinishedReceiving = 0x00190000,
		SetInputLineWidth = 0x001A0040,
		GetInputLineWidth = 0x001B0000,
		SetInputLines = 0x001C0040,
		GetInputLines = 0x001D0000,
		SetCoefficientParams = 0x001E0100,
		GetCoefficientParams = 0x001F0000,
		SetStandardCoeff = 0x00200040,
		GetStandardCoefficientParams = 0x00210040,
		SetAlpha = 0x00220040,
		StartConversion = 0x00260000,
		StopConversion = 0x00270000,
		IsBusyConversion = 0x00280000,
		SetPackageParameter = 0x002901C0,
		PingProcess = 0x002A0000,
		DriverInitialize = 0x002B0000,
		DriverFinalize = 0x002C0000,
	};
}

void Y2RService::reset() {
	transferEndInterruptEnabled = false;
	transferEndEvent = std::nullopt;
	alignment = BlockAlignment::Line;
	inputFmt = InputFormat::YUV422_Individual8;
	outputFmt = OutputFormat::RGB32;
	rotation = Rotation::None;
	spacialDithering = false;
	temporalDithering = false;
	alpha = 0xFFFF;
	inputLines = 0;
	inputLineWidth = 0;
	conversionCoefficients.fill(0);
	isBusy = false;
}

void Y2RService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	IPC::RequestParser rp(messagePointer, mem);

	switch (command) {
		case Y2RCommands::DriverInitialize: driverInitialize(messagePointer); break;
		case Y2RCommands::DriverFinalize: driverFinalize(messagePointer); break;
		case Y2RCommands::StartConversion: {
			log("Y2R::StartConversion\n");
			isBusy = true;
			// Real Functioning: In a real emulator, we'd perform conversion here.
			// For now, we signal completion immediately if an event is registered.
			if (transferEndInterruptEnabled && transferEndEvent) {
				kernel.signalEvent(transferEndEvent.value());
			}
			isBusy = false;
			auto rb = rp.MakeBuilder(1, 0);
			rb.Push(Result::Success);
			break;
		}
		case Y2RCommands::IsBusyConversion: {
			auto rb = rp.MakeBuilder(2, 0);
			rb.Push(Result::Success);
			rb.Push(isBusy ? 1u : 0u);
			break;
		}
		case Y2RCommands::SetInputFormat: {
			inputFmt = static_cast<InputFormat>(rp.Pop());
			auto rb = rp.MakeBuilder(1, 0);
			rb.Push(Result::Success);
			break;
		}
		case Y2RCommands::SetOutputFormat: {
			outputFmt = static_cast<OutputFormat>(rp.Pop());
			auto rb = rp.MakeBuilder(1, 0);
			rb.Push(Result::Success);
			break;
		}
		case Y2RCommands::GetTransferEndEvent: {
			if (!transferEndEvent) transferEndEvent = kernel.makeEvent(ResetType::OneShot);
			auto rb = rp.MakeBuilder(1, 2);
			rb.Push(Result::Success);
			rb.Push(IPC::CopyHandleDesc(1));
			rb.Push(transferEndEvent.value());
			break;
		}
		case Y2RCommands::SetTransferEndInterrupt: {
			transferEndInterruptEnabled = rp.Pop() != 0;
			auto rb = rp.MakeBuilder(1, 0);
			rb.Push(Result::Success);
			break;
		}
		default:
			log("Y2R service requested unknown command: %08X\n", command);
			auto rb = rp.MakeBuilder(1, 0);
			rb.Push(Result::Success);
			break;
	}
}

// Stub implementation for other functions to satisfy registration
void Y2RService::driverInitialize(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void Y2RService::driverFinalize(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void Y2RService::getBlockAlignment(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void Y2RService::getInputLines(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void Y2RService::getInputLineWidth(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void Y2RService::getOutputFormat(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void Y2RService::getTransferEndEvent(u32 messagePointer) { /* Handled in switch */ }
void Y2RService::getStandardCoefficientParams(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void Y2RService::isBusyConversion(u32 messagePointer) { /* Handled in switch */ }
void Y2RService::isFinishedReceiving(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void Y2RService::isFinishedSendingY(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void Y2RService::isFinishedSendingU(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void Y2RService::isFinishedSendingV(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void Y2RService::isFinishedSendingYUV(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void Y2RService::pingProcess(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void Y2RService::setAlpha(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void Y2RService::setBlockAlignment(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void Y2RService::setInputFormat(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void Y2RService::setInputLineWidth(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void Y2RService::setInputLines(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void Y2RService::setOutputFormat(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void Y2RService::setPackageParameter(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void Y2RService::setReceiving(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void Y2RService::setRotation(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void Y2RService::setSendingY(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void Y2RService::setSendingU(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void Y2RService::setSendingV(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void Y2RService::setSendingYUV(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void Y2RService::setSpacialDithering(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void Y2RService::setStandardCoeff(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void Y2RService::setTemporalDithering(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
