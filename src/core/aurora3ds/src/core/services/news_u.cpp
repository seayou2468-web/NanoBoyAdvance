#include "../../../include/services/news_u.hpp"

#include "../../../include/ipc.hpp"

namespace NewsCommands {
	enum : u32 {
		AddNotification = 0x000100C8,
		AddNotificationSystem = 0x000100C6,
		ResetNotifications = 0x00040000,
		GetTotalNotifications = 0x00050000,
		SetNewsDBHeader = 0x00060042,
		SetNotificationHeader = 0x00070082,
		SetNotificationMessage = 0x00080082,
		SetNotificationImage = 0x00090082,
		GetNewsDBHeader = 0x000A0042,
		GetNotificationHeader = 0x000B0082,
		GetNotificationMessage = 0x000C0082,
		GetNotificationImage = 0x000D0082,
		SetAutomaticSyncFlag = 0x00110040,
		SetNotificationHeaderOther = 0x00120082,
		WriteNewsDBSavedata = 0x00130000,
		GetTotalArrivedNotifications = 0x00140000,
	};
}

void NewsUService::reset() {
	totalNotifications = 0;
	automaticSyncFlag = false;
	messageSizes.fill(0);
	imageSizes.fill(0);
}

void NewsUService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case NewsCommands::AddNotification: addNotification(messagePointer); break;
		case NewsCommands::AddNotificationSystem: addNotificationSystem(messagePointer); break;
		case NewsCommands::ResetNotifications: resetNotifications(messagePointer); break;
		case NewsCommands::GetTotalNotifications: getTotalNotifications(messagePointer); break;
		case NewsCommands::SetNewsDBHeader: setNewsDBHeader(messagePointer); break;
		case NewsCommands::SetNotificationHeader: setNotificationHeader(messagePointer); break;
		case NewsCommands::SetNotificationMessage: setNotificationMessage(messagePointer); break;
		case NewsCommands::SetNotificationImage: setNotificationImage(messagePointer); break;
		case NewsCommands::GetNewsDBHeader: getNewsDBHeader(messagePointer); break;
		case NewsCommands::GetNotificationHeader: getNotificationHeader(messagePointer); break;
		case NewsCommands::GetNotificationMessage: getNotificationMessage(messagePointer); break;
		case NewsCommands::GetNotificationImage: getNotificationImage(messagePointer); break;
		case NewsCommands::SetAutomaticSyncFlag: setAutomaticSyncFlag(messagePointer); break;
		case NewsCommands::SetNotificationHeaderOther: setNotificationHeaderOther(messagePointer); break;
		case NewsCommands::WriteNewsDBSavedata: writeNewsDBSavedata(messagePointer); break;
		case NewsCommands::GetTotalArrivedNotifications: getTotalArrivedNotifications(messagePointer); break;
		default: Helpers::panic("news service requested. Command: %08X\n", command);
	}
}

void NewsUService::addNotification(u32 messagePointer) {
	log("NEWS::AddNotification (stubbed)\n");
	if (totalNotifications < MaxNotifications) {
		totalNotifications++;
	}

	mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NewsUService::addNotificationSystem(u32 messagePointer) {
	log("NEWS::AddNotificationSystem (stubbed)\n");
	addNotification(messagePointer);
}

void NewsUService::resetNotifications(u32 messagePointer) {
	log("NEWS::ResetNotifications (stubbed)\n");
	reset();

	mem.write32(messagePointer, IPC::responseHeader(0x4, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NewsUService::getTotalNotifications(u32 messagePointer) {
	log("NEWS::GetTotalNotifications\n");

	mem.write32(messagePointer, IPC::responseHeader(0x5, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, totalNotifications);
}

void NewsUService::setNewsDBHeader(u32 messagePointer) {
	log("NEWS::SetNewsDBHeader (stubbed)\n");

	mem.write32(messagePointer, IPC::responseHeader(0x6, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NewsUService::setNotificationHeader(u32 messagePointer) {
	const u32 index = mem.read32(messagePointer + 4);
	log("NEWS::SetNotificationHeader (index=%u) (stubbed)\n", index);

	mem.write32(messagePointer, IPC::responseHeader(0x7, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NewsUService::setNotificationMessage(u32 messagePointer) {
	const u32 index = mem.read32(messagePointer + 4);
	const u32 size = mem.read32(messagePointer + 8);
	log("NEWS::SetNotificationMessage (index=%u, size=%u) (stubbed)\n", index, size);

	if (index < MaxNotifications) {
		messageSizes[index] = size;
	}

	mem.write32(messagePointer, IPC::responseHeader(0x8, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NewsUService::setNotificationImage(u32 messagePointer) {
	const u32 index = mem.read32(messagePointer + 4);
	const u32 size = mem.read32(messagePointer + 8);
	log("NEWS::SetNotificationImage (index=%u, size=%u) (stubbed)\n", index, size);

	if (index < MaxNotifications) {
		imageSizes[index] = size;
	}

	mem.write32(messagePointer, IPC::responseHeader(0x9, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NewsUService::getNewsDBHeader(u32 messagePointer) {
	const u32 outSize = mem.read32(messagePointer + 4);
	log("NEWS::GetNewsDBHeader (size=%u) (stubbed)\n", outSize);

	mem.write32(messagePointer, IPC::responseHeader(0xA, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0x10);
}

void NewsUService::getNotificationHeader(u32 messagePointer) {
	const u32 index = mem.read32(messagePointer + 4);
	const u32 outSize = mem.read32(messagePointer + 8);
	log("NEWS::GetNotificationHeader (index=%u, size=%u) (stubbed)\n", index, outSize);

	mem.write32(messagePointer, IPC::responseHeader(0xB, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0x70);
}

void NewsUService::getNotificationMessage(u32 messagePointer) {
	const u32 index = mem.read32(messagePointer + 4);
	const u32 outSize = mem.read32(messagePointer + 8);
	log("NEWS::GetNotificationMessage (index=%u, size=%u) (stubbed)\n", index, outSize);

	const u32 returnedSize = (index < MaxNotifications) ? messageSizes[index] : 0;
	mem.write32(messagePointer, IPC::responseHeader(0xC, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, returnedSize);
}

void NewsUService::getNotificationImage(u32 messagePointer) {
	const u32 index = mem.read32(messagePointer + 4);
	const u32 outSize = mem.read32(messagePointer + 8);
	log("NEWS::GetNotificationImage (index=%u, size=%u) (stubbed)\n", index, outSize);

	const u32 returnedSize = (index < MaxNotifications) ? imageSizes[index] : 0;
	mem.write32(messagePointer, IPC::responseHeader(0xD, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, returnedSize);
}

void NewsUService::setAutomaticSyncFlag(u32 messagePointer) {
	automaticSyncFlag = mem.read32(messagePointer + 4) != 0;
	log("NEWS::SetAutomaticSyncFlag (flag=%u) (stubbed)\n", automaticSyncFlag ? 1 : 0);

	mem.write32(messagePointer, IPC::responseHeader(0x11, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NewsUService::setNotificationHeaderOther(u32 messagePointer) {
	const u32 index = mem.read32(messagePointer + 4);
	log("NEWS::SetNotificationHeaderOther (index=%u) (stubbed)\n", index);

	mem.write32(messagePointer, IPC::responseHeader(0x12, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NewsUService::writeNewsDBSavedata(u32 messagePointer) {
	log("NEWS::WriteNewsDBSavedata (stubbed)\n");

	mem.write32(messagePointer, IPC::responseHeader(0x13, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NewsUService::getTotalArrivedNotifications(u32 messagePointer) {
	log("NEWS::GetTotalArrivedNotifications (stubbed)\n");

	mem.write32(messagePointer, IPC::responseHeader(0x14, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, totalNotifications);
}
