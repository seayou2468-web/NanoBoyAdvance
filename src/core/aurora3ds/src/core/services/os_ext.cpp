#include "../../../include/services/os_ext.hpp"

#include <algorithm>

#include "../../../include/ipc.hpp"
#include "../../../include/kernel/kernel.hpp"

void OSExtService::reset() {
	state = {};
	eventHandle = std::nullopt;
}

void OSExtService::handleSyncRequest(u32 messagePointer) {
	const u32 rawCommand = mem.read32(messagePointer);
	const u32 commandID = rawCommand >> 16;

	switch (static_cast<GenericCommand>(commandID)) {
		case GenericCommand::Initialize: initialize(messagePointer); break;
		case GenericCommand::GetState: getState(messagePointer); break;
		case GenericCommand::SetState: setState(messagePointer); break;
		case GenericCommand::ReadRegister: readRegister(messagePointer); break;
		case GenericCommand::WriteRegister: writeRegister(messagePointer); break;
		case GenericCommand::GetVersion: getVersion(messagePointer); break;
		case GenericCommand::Reset: resetState(messagePointer); break;
		case GenericCommand::GetEventHandle: getEvent(messagePointer); break;
		case GenericCommand::ReadBlob: readBlob(messagePointer); break;
		case GenericCommand::WriteBlob: writeBlob(messagePointer); break;
		case GenericCommand::GetTicks: getTicks(messagePointer); break;
		default: respondDefault(messagePointer, commandID); break;
	}
}

void OSExtService::respondDefault(u32 messagePointer, u32 commandID) {
	// Keep unknown commands running with a deterministic, stateful response.
	const u32 slot = commandID & 0x3F;
	state.registers[slot] ^= (0x9E3779B9u ^ commandID);
	mem.write32(messagePointer, IPC::responseHeader(commandID, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, state.registers[slot]);
}

void OSExtService::initialize(u32 messagePointer) {
	mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void OSExtService::getState(u32 messagePointer) {
	mem.write32(messagePointer, IPC::responseHeader(0x2, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, state.flags);
}

void OSExtService::setState(u32 messagePointer) {
	state.flags = mem.read32(messagePointer + 4);
	mem.write32(messagePointer, IPC::responseHeader(0x3, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void OSExtService::readRegister(u32 messagePointer) {
	const u32 index = mem.read32(messagePointer + 4) & 0x3F;
	mem.write32(messagePointer, IPC::responseHeader(0x4, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, state.registers[index]);
}

void OSExtService::writeRegister(u32 messagePointer) {
	const u32 index = mem.read32(messagePointer + 4) & 0x3F;
	const u32 value = mem.read32(messagePointer + 8);
	state.registers[index] = value;
	mem.write32(messagePointer, IPC::responseHeader(0x5, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void OSExtService::getVersion(u32 messagePointer) {
	mem.write32(messagePointer, IPC::responseHeader(0x6, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, state.version);
}

void OSExtService::resetState(u32 messagePointer) {
	state = {};
	mem.write32(messagePointer, IPC::responseHeader(0x7, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void OSExtService::getEvent(u32 messagePointer) {
	if (!eventHandle.has_value() || kernel.getObject(eventHandle.value(), KernelObjectType::Event) == nullptr) {
		eventHandle = kernel.makeEvent(ResetType::OneShot);
	}

	mem.write32(messagePointer, IPC::responseHeader(0x8, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0);
	mem.write32(messagePointer + 12, eventHandle.value());
}

void OSExtService::readBlob(u32 messagePointer) {
	const u32 key = mem.read32(messagePointer + 4);
	const u32 requestedSize = std::min<u32>(mem.read32(messagePointer + 8), 256);
	const u32 outBuffer = mem.read32(messagePointer + 0x100 + 4);
	const auto it = state.blobStore.find(key);
	const u32 copySize = (it == state.blobStore.end()) ? 0 : requestedSize;

	if (it != state.blobStore.end()) {
		for (u32 i = 0; i < copySize; i++) {
			mem.write8(outBuffer + i, it->second[i]);
		}
	}

	mem.write32(messagePointer, IPC::responseHeader(0x9, 2, 2));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, copySize);
}

void OSExtService::writeBlob(u32 messagePointer) {
	const u32 key = mem.read32(messagePointer + 4);
	const u32 requestedSize = std::min<u32>(mem.read32(messagePointer + 8), 256);
	const u32 inBuffer = mem.read32(messagePointer + 0x100 + 4);
	auto& blob = state.blobStore[key];

	for (u32 i = 0; i < requestedSize; i++) {
		blob[i] = mem.read8(inBuffer + i);
	}

	mem.write32(messagePointer, IPC::responseHeader(0xA, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void OSExtService::getTicks(u32 messagePointer) {
	// Use deterministic values derived from service state to avoid host clock dependencies.
	const u64 ticks = (static_cast<u64>(state.flags) << 32) | state.registers[0];
	mem.write32(messagePointer, IPC::responseHeader(0xB, 3, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, static_cast<u32>(ticks & 0xFFFFFFFFu));
	mem.write32(messagePointer + 12, static_cast<u32>(ticks >> 32));
}
