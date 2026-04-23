#include "../../../include/services/news_u.hpp"

#include <algorithm>

#include "../../../include/ipc.hpp"

namespace NewsCommands {
	enum : u32 {
		AddNotification = 0x0001,
		ResetNotifications = 0x0004,
		GetTotalNotifications = 0x0005,
		SetNewsDBHeader = 0x0006,
		SetNotificationHeader = 0x0007,
		SetNotificationMessage = 0x0008,
		SetNotificationImage = 0x0009,
		GetNewsDBHeader = 0x000A,
		GetNotificationHeader = 0x000B,
		GetNotificationMessage = 0x000C,
		GetNotificationImage = 0x000D,
		SetInfoLEDPattern = 0x000E,
		SyncArrivedNotifications = 0x000F,
		SyncOneArrivedNotification = 0x0010,
		SetAutomaticSyncFlag = 0x0011,
		SetNotificationHeaderOther = 0x0012,
		WriteNewsDBSavedata = 0x0013,
		GetTotalArrivedNotifications = 0x0014,
	};
}

void NewsUService::reset() {
	totalNotifications = 0;
	automaticSyncFlag = false;
	for (auto& header : notificationHeaders) {
		header.fill(0);
	}
	newsDBHeader.fill(0);
	for (auto& message : messages) {
		message.clear();
	}
	for (auto& image : images) {
		image.clear();
	}
	messageSizes.fill(0);
	imageSizes.fill(0);
}

void NewsUService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command >> 16) {
		case NewsCommands::AddNotification: addNotification(messagePointer); break;
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
		case NewsCommands::SetInfoLEDPattern:
			log("NEWS::SetInfoLEDPattern (stubbed)\n");
			mem.write32(messagePointer, IPC::responseHeader(0xE, 1, 0));
			mem.write32(messagePointer + 4, Result::Success);
			break;
		case NewsCommands::SyncArrivedNotifications: syncArrivedNotifications(messagePointer); break;
		case NewsCommands::SyncOneArrivedNotification: syncOneArrivedNotification(messagePointer); break;
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

void NewsUService::syncArrivedNotifications(u32 messagePointer) {
	log("NEWS::SyncArrivedNotifications (stubbed)\n");
	mem.write32(messagePointer, IPC::responseHeader(0xF, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NewsUService::syncOneArrivedNotification(u32 messagePointer) {
	log("NEWS::SyncOneArrivedNotification (stubbed)\n");
	mem.write32(messagePointer, IPC::responseHeader(0x10, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
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
	const u32 size = mem.read32(messagePointer + 4);
	const u32 pointer = mem.read32(messagePointer + 12);
	log("NEWS::SetNewsDBHeader (stubbed)\n");
	const u32 copySize = std::min<u32>(size, static_cast<u32>(newsDBHeader.size()));
	for (u32 i = 0; i < copySize; i++) {
		newsDBHeader[i] = mem.read8(pointer + i);
	}

	mem.write32(messagePointer, IPC::responseHeader(0x6, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NewsUService::setNotificationHeader(u32 messagePointer) {
	const u32 index = mem.read32(messagePointer + 4);
	const u32 size = mem.read32(messagePointer + 8);
	const u32 pointer = mem.read32(messagePointer + 16);
	log("NEWS::SetNotificationHeader (index=%u) (stubbed)\n", index);
	if (index < MaxNotifications) {
		const u32 copySize = std::min<u32>(size, static_cast<u32>(notificationHeaders[index].size()));
		for (u32 i = 0; i < copySize; i++) {
			notificationHeaders[index][i] = mem.read8(pointer + i);
		}
	}

	mem.write32(messagePointer, IPC::responseHeader(0x7, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NewsUService::setNotificationMessage(u32 messagePointer) {
	const u32 index = mem.read32(messagePointer + 4);
	const u32 size = mem.read32(messagePointer + 8);
	const u32 pointer = mem.read32(messagePointer + 16);
	log("NEWS::SetNotificationMessage (index=%u, size=%u) (stubbed)\n", index, size);

	if (index < MaxNotifications) {
		messages[index].resize(size);
		for (u32 i = 0; i < size; i++) {
			messages[index][i] = mem.read8(pointer + i);
		}
		messageSizes[index] = size;
	}

	mem.write32(messagePointer, IPC::responseHeader(0x8, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NewsUService::setNotificationImage(u32 messagePointer) {
	const u32 index = mem.read32(messagePointer + 4);
	const u32 size = mem.read32(messagePointer + 8);
	const u32 pointer = mem.read32(messagePointer + 16);
	log("NEWS::SetNotificationImage (index=%u, size=%u) (stubbed)\n", index, size);

	if (index < MaxNotifications) {
		images[index].resize(size);
		for (u32 i = 0; i < size; i++) {
			images[index][i] = mem.read8(pointer + i);
		}
		imageSizes[index] = size;
	}

	mem.write32(messagePointer, IPC::responseHeader(0x9, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NewsUService::getNewsDBHeader(u32 messagePointer) {
	const u32 outSize = mem.read32(messagePointer + 4);
	const u32 pointer = mem.read32(messagePointer + 12);
	log("NEWS::GetNewsDBHeader (size=%u) (stubbed)\n", outSize);
	const u32 copySize = std::min<u32>(outSize, static_cast<u32>(newsDBHeader.size()));
	for (u32 i = 0; i < copySize; i++) {
		mem.write8(pointer + i, newsDBHeader[i]);
	}

	mem.write32(messagePointer, IPC::responseHeader(0xA, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, copySize);
}

void NewsUService::getNotificationHeader(u32 messagePointer) {
	const u32 index = mem.read32(messagePointer + 4);
	const u32 outSize = mem.read32(messagePointer + 8);
	const u32 pointer = mem.read32(messagePointer + 16);
	log("NEWS::GetNotificationHeader (index=%u, size=%u) (stubbed)\n", index, outSize);
	u32 copySize = 0;
	if (index < MaxNotifications) {
		copySize = std::min<u32>(outSize, static_cast<u32>(notificationHeaders[index].size()));
		for (u32 i = 0; i < copySize; i++) {
			mem.write8(pointer + i, notificationHeaders[index][i]);
		}
	}

	mem.write32(messagePointer, IPC::responseHeader(0xB, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, copySize);
}

void NewsUService::getNotificationMessage(u32 messagePointer) {
	const u32 index = mem.read32(messagePointer + 4);
	const u32 outSize = mem.read32(messagePointer + 8);
	const u32 pointer = mem.read32(messagePointer + 16);
	log("NEWS::GetNotificationMessage (index=%u, size=%u) (stubbed)\n", index, outSize);

	const u32 returnedSize = (index < MaxNotifications) ? messageSizes[index] : 0;
	if (index < MaxNotifications) {
		const u32 copySize = std::min<u32>(outSize, static_cast<u32>(messages[index].size()));
		for (u32 i = 0; i < copySize; i++) {
			mem.write8(pointer + i, messages[index][i]);
		}
	}
	mem.write32(messagePointer, IPC::responseHeader(0xC, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, returnedSize);
}

void NewsUService::getNotificationImage(u32 messagePointer) {
	const u32 index = mem.read32(messagePointer + 4);
	const u32 outSize = mem.read32(messagePointer + 8);
	const u32 pointer = mem.read32(messagePointer + 16);
	log("NEWS::GetNotificationImage (index=%u, size=%u) (stubbed)\n", index, outSize);

	const u32 returnedSize = (index < MaxNotifications) ? imageSizes[index] : 0;
	if (index < MaxNotifications) {
		const u32 copySize = std::min<u32>(outSize, static_cast<u32>(images[index].size()));
		for (u32 i = 0; i < copySize; i++) {
			mem.write8(pointer + i, images[index][i]);
		}
	}
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
