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
	nextNotificationIndex = 0;
	for (auto& header : notificationHeaders) {
		header.fill(0);
	}
	for (auto& header : notificationHeadersOther) {
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

void NewsUService::handleSyncRequest(u32 messagePointer, Type type) {
	const u32 command = mem.read32(messagePointer);
	switch (command >> 16) {
		case NewsCommands::AddNotification:
			if (type == Type::S) {
				addNotificationSystem(messagePointer);
			} else {
				addNotification(messagePointer, false);
			}
			break;
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

void NewsUService::addNotification(u32 messagePointer, bool systemNotification) {
	const u32 headerSize = mem.read32(messagePointer + 4);
	const u32 messageSize = mem.read32(messagePointer + 8);
	const u32 imageSize = mem.read32(messagePointer + 12);
	const u32 mappedBufferOffset = systemNotification ? 16 : 24;
	const u32 headerPointer = mem.read32(messagePointer + mappedBufferOffset);
	const u32 messagePtr = mem.read32(messagePointer + mappedBufferOffset + 8);
	const u32 imagePointer = mem.read32(messagePointer + mappedBufferOffset + 16);

	log(
		"NEWS::AddNotification%s (header=%u bytes, message=%u bytes, image=%u bytes)\n",
		systemNotification ? "System" : "", headerSize, messageSize, imageSize
	);

	const u32 index = nextNotificationIndex;
	nextNotificationIndex = (nextNotificationIndex + 1) % MaxNotifications;

	const u32 headerCopySize = std::min<u32>(headerSize, static_cast<u32>(notificationHeaders[index].size()));
	for (u32 i = 0; i < headerCopySize; i++) {
		notificationHeaders[index][i] = mem.read8(headerPointer + i);
	}
	// Ensure notification is marked valid if the caller didn't initialize it.
	notificationHeaders[index][0] = 1;

	messages[index].resize(messageSize);
	for (u32 i = 0; i < messageSize; i++) {
		messages[index][i] = mem.read8(messagePtr + i);
	}
	messageSizes[index] = messageSize;

	images[index].resize(imageSize);
	for (u32 i = 0; i < imageSize; i++) {
		images[index][i] = mem.read8(imagePointer + i);
	}
	imageSizes[index] = imageSize;

	if (totalNotifications < MaxNotifications) {
		totalNotifications++;
	}

	mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NewsUService::addNotificationSystem(u32 messagePointer) {
	log("NEWS::AddNotificationSystem\n");
	addNotification(messagePointer, true);
}

void NewsUService::syncArrivedNotifications(u32 messagePointer) {
	log("NEWS::SyncArrivedNotifications\n");
	mem.write32(messagePointer, IPC::responseHeader(0xF, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NewsUService::syncOneArrivedNotification(u32 messagePointer) {
	log("NEWS::SyncOneArrivedNotification\n");
	mem.write32(messagePointer, IPC::responseHeader(0x10, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NewsUService::resetNotifications(u32 messagePointer) {
	log("NEWS::ResetNotifications\n");
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
	log("NEWS::SetNewsDBHeader\n");
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
	log("NEWS::SetNotificationHeader (index=%u)\n", index);
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
	log("NEWS::SetNotificationMessage (index=%u, size=%u)\n", index, size);

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
	log("NEWS::SetNotificationImage (index=%u, size=%u)\n", index, size);

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
	log("NEWS::GetNewsDBHeader (size=%u)\n", outSize);
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
	log("NEWS::GetNotificationHeader (index=%u, size=%u)\n", index, outSize);
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
	log("NEWS::GetNotificationMessage (index=%u, size=%u)\n", index, outSize);

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
	log("NEWS::GetNotificationImage (index=%u, size=%u)\n", index, outSize);

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
	log("NEWS::SetAutomaticSyncFlag (flag=%u)\n", automaticSyncFlag ? 1 : 0);

	mem.write32(messagePointer, IPC::responseHeader(0x11, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NewsUService::setNotificationHeaderOther(u32 messagePointer) {
	const u32 index = mem.read32(messagePointer + 4);
	const u32 size = mem.read32(messagePointer + 8);
	const u32 pointer = mem.read32(messagePointer + 16);
	log("NEWS::SetNotificationHeaderOther (index=%u, size=%u)\n", index, size);

	if (index < MaxNotifications) {
		const u32 copySize = std::min<u32>(size, static_cast<u32>(notificationHeadersOther[index].size()));
		for (u32 i = 0; i < copySize; i++) {
			notificationHeadersOther[index][i] = mem.read8(pointer + i);
		}
	}

	mem.write32(messagePointer, IPC::responseHeader(0x12, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NewsUService::writeNewsDBSavedata(u32 messagePointer) {
	log("NEWS::WriteNewsDBSavedata\n");

	mem.write32(messagePointer, IPC::responseHeader(0x13, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NewsUService::getTotalArrivedNotifications(u32 messagePointer) {
	log("NEWS::GetTotalArrivedNotifications\n");

	mem.write32(messagePointer, IPC::responseHeader(0x14, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, totalNotifications);
}
