#include "../../../include/services/reference_fallback_bridge.hpp"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "../../../include/ipc.hpp"
#include "../../../include/kernel/handles.hpp"
#include "../../../include/memory.hpp"
#include "../../../include/result/result.hpp"

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

	u32 effectiveLength = nameLength;
	if (effectiveLength == 0 || effectiveLength > 8) {
		effectiveLength = nulTerminator;
	}
	effectiveLength = std::min<u32>(effectiveLength, nulTerminator);
	if (effectiveLength == 0) {
		return std::nullopt;
	}

	return std::string(rawName.data(), rawName.data() + effectiveLength);
}

constexpr u32 CommandRegisterClient = 0x00010002;
constexpr u32 CommandGetServiceHandle = 0x00050100;

const std::unordered_map<std::string_view, HorizonHandle> kServiceMap{
	{"ac:u", KernelHandles::AC},
	{"act:a", KernelHandles::ACT},
	{"am:app", KernelHandles::AM},
	{"apt:u", KernelHandles::APT},
	{"boss:U", KernelHandles::BOSS},
	{"cam:u", KernelHandles::CAM},
	{"cecd:u", KernelHandles::CECD},
	{"cfg:u", KernelHandles::CFG_U},
	{"csnd:SND", KernelHandles::CSND},
	{"dsp::DSP", KernelHandles::DSP},
	{"err:f", KernelHandles::ERR_F},
	{"frd:a", KernelHandles::FRD_A},
	{"frd:u", KernelHandles::FRD_U},
	{"fs:USER", KernelHandles::FS},
	{"gsp::Gpu", KernelHandles::GPU},
	{"hid:USER", KernelHandles::HID},
	{"http:C", KernelHandles::HTTP},
	{"ir:USER", KernelHandles::IR_USER},
	{"lcd:U", KernelHandles::LCD},
	{"ldr:ro", KernelHandles::LDR_RO},
	{"mcu::HWC", KernelHandles::MCU_HWC},
	{"mic:u", KernelHandles::MIC},
	{"ndm:u", KernelHandles::NDM},
	{"news:s", KernelHandles::NEWS_S},
	{"news:u", KernelHandles::NEWS_U},
	{"nfc:m", KernelHandles::NFC},
	{"nim:aoc", KernelHandles::NIM},
	{"ns:s", KernelHandles::NS_S},
	{"ptm:u", KernelHandles::PTM_U},
	{"pxi:dev", KernelHandles::PXI_DEV},
	{"soc:U", KernelHandles::SOC},
	{"ssl:C", KernelHandles::SSL},
	{"y2r:u", KernelHandles::Y2R},
};

bool HandleService(void* memoryCtx, u32 serviceHandle, u32 messagePointer) {
	if (memoryCtx == nullptr) {
		return false;
	}

	auto& mem = *static_cast<Memory*>(memoryCtx);
	const u32 commandWord = mem.read32(messagePointer);
	const u32 commandId = commandWord >> 16;

	Helpers::warn(
		"[REF-BUILTIN] unhandled service=%08X cmd=%08X id=%04X -> NotImplemented",
		serviceHandle, commandWord, commandId
	);
	mem.write32(messagePointer, IPC::responseHeader(commandId, 1, 0));
	mem.write32(messagePointer + 4, Result::OS::NotImplemented);
	return true;
}

bool HandlePort(void* memoryCtx, const char* portName, u32 messagePointer) {
	if (memoryCtx == nullptr) {
		return false;
	}

	auto& mem = *static_cast<Memory*>(memoryCtx);
	const std::string_view port = (portName != nullptr) ? std::string_view(portName) : std::string_view{};
	const u32 header = mem.read32(messagePointer);

	if (port != "srv:") {
		const u32 commandId = header >> 16;
		mem.write32(messagePointer, IPC::responseHeader(commandId, 1, 0));
		mem.write32(messagePointer + 4, Result::OS::NotImplemented);
		return true;
	}

	switch (header) {
		case CommandRegisterClient:
			mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 0));
			mem.write32(messagePointer + 4, Result::Success);
			return true;
		case CommandGetServiceHandle: {
			const u32 nameLength = mem.read32(messagePointer + 12);
			const auto serviceOpt = readIPCName(mem, messagePointer + 4, nameLength);
			if (!serviceOpt.has_value()) {
				mem.write32(messagePointer, IPC::responseHeader(0x5, 1, 2));
				mem.write32(messagePointer + 4, Result::OS::InvalidCombination);
				mem.write32(messagePointer + 8, 0);
				mem.write32(messagePointer + 12, 0);
				return true;
			}

			if (auto it = kServiceMap.find(*serviceOpt); it != kServiceMap.end()) {
				mem.write32(messagePointer, IPC::responseHeader(0x5, 1, 2));
				mem.write32(messagePointer + 4, Result::Success);
				mem.write32(messagePointer + 8, 0x04000000);
				mem.write32(messagePointer + 12, it->second);
				return true;
			}

			mem.write32(messagePointer, IPC::responseHeader(0x5, 1, 2));
			mem.write32(messagePointer + 4, Result::OS::NotImplemented);
			mem.write32(messagePointer + 8, 0);
			mem.write32(messagePointer + 12, 0);
			return true;
		}
		default: {
			const u32 commandId = header >> 16;
			mem.write32(messagePointer, IPC::responseHeader(commandId, 1, 0));
			mem.write32(messagePointer + 4, Result::OS::NotImplemented);
			return true;
		}
	}
}
}  // namespace

void NBAReferenceFallbackRegisterBuiltinHandlers() {
	// Built-in fallback: deterministic handlers (no success-style generic stubs).
	NBAReferenceFallbackRegisterHandlers(&HandleService, &HandlePort);
}
