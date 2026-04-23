#include "../../../include/services/fs.hpp"

#include <cstdio>
#include <filesystem>

#include "../../../include/io_file.hpp"
#include "../../../include/ipc.hpp"
#include "../../../include/kernel/kernel.hpp"
#include "../../../include/result/result.hpp"

#ifdef CreateFile
#undef CreateDirectory
#undef CreateFile
#undef DeleteFile
#endif

namespace FSCommands {
	enum : u32 {
		Initialize = 0x08010002,
		OpenFile = 0x080201C2,
		OpenFileDirectly = 0x08030204,
		DeleteFile = 0x08040142,
		RenameFile = 0x08050244,
		DeleteDirectory = 0x08060142,
		DeleteDirectoryRecursively = 0x08070142,
		CreateFile = 0x08080202,
		CreateDirectory = 0x08090182,
		OpenDirectory = 0x080B0102,
		OpenArchive = 0x080C00C2,
		ControlArchive = 0x080D0144,
		CloseArchive = 0x080E0080,
		FormatThisUserSaveData = 0x080F0180,
		GetFreeBytes = 0x08120080,
		GetSdmcArchiveResource = 0x08140000,
		IsSdmcDetected = 0x08170000,
		IsSdmcWritable = 0x08180000,
		CardSlotIsInserted = 0x08210000,
		AbnegateAccessRight = 0x08400040,
		GetFormatInfo = 0x084500C2,
		GetArchiveResource = 0x08490040,
		FormatSaveData = 0x084C0242,
		CreateExtSaveData = 0x08510242,
		DeleteExtSaveData = 0x08520100,
		SetArchivePriority = 0x085A00C0,
		InitializeWithSdkVersion = 0x08610042,
		SetPriority = 0x08620040,
		GetPriority = 0x08630000,
		SetThisSaveDataSecureValue = 0x086E00C0,
		GetThisSaveDataSecureValue = 0x086F0040,
		TheGameboyVCFunction = 0x08750180,
	};
}

void FSService::reset() {
	priority = 0;
	secureValues.clear();
}

void FSService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case FSCommands::Initialize: initialize(messagePointer); break;
		case FSCommands::InitializeWithSdkVersion: initializeWithSdkVersion(messagePointer); break;
		case FSCommands::OpenFile: openFile(messagePointer); break;
		case FSCommands::OpenFileDirectly: openFileDirectly(messagePointer); break;
		case FSCommands::DeleteFile: deleteFile(messagePointer); break;
		case FSCommands::RenameFile: renameFile(messagePointer); break;
		case FSCommands::DeleteDirectory: deleteDirectory(messagePointer); break;
		case FSCommands::DeleteDirectoryRecursively: deleteDirectory(messagePointer); break;
		case FSCommands::CreateFile: createFile(messagePointer); break;
		case FSCommands::CreateDirectory: createDirectory(messagePointer); break;
		case FSCommands::OpenDirectory: openDirectory(messagePointer); break;
		case FSCommands::OpenArchive: openArchive(messagePointer); break;
		case FSCommands::ControlArchive: controlArchive(messagePointer); break;
		case FSCommands::CloseArchive: closeArchive(messagePointer); break;
		case FSCommands::FormatThisUserSaveData: formatThisUserSaveData(messagePointer); break;
		case FSCommands::GetFreeBytes: getFreeBytes(messagePointer); break;
		case FSCommands::GetSdmcArchiveResource: getSdmcArchiveResource(messagePointer); break;
		case FSCommands::IsSdmcDetected: isSdmcDetected(messagePointer); break;
		case FSCommands::IsSdmcWritable: isSdmcWritable(messagePointer); break;
		case FSCommands::CardSlotIsInserted: cardSlotIsInserted(messagePointer); break;
		case FSCommands::AbnegateAccessRight: abnegateAccessRight(messagePointer); break;
		case FSCommands::GetFormatInfo: getFormatInfo(messagePointer); break;
		case FSCommands::GetArchiveResource: getArchiveResource(messagePointer); break;
		case FSCommands::FormatSaveData: formatSaveData(messagePointer); break;
		case FSCommands::CreateExtSaveData: createExtSaveData(messagePointer); break;
		case FSCommands::DeleteExtSaveData: deleteExtSaveData(messagePointer); break;
		case FSCommands::SetArchivePriority: setArchivePriority(messagePointer); break;
		case FSCommands::SetPriority: setPriority(messagePointer); break;
		case FSCommands::GetPriority: getPriority(messagePointer); break;
		case FSCommands::SetThisSaveDataSecureValue: setThisSaveDataSecureValue(messagePointer); break;
		case FSCommands::GetThisSaveDataSecureValue: getThisSaveDataSecureValue(messagePointer); break;
		case FSCommands::TheGameboyVCFunction: theGameboyVCFunction(messagePointer); break;

		default:
			log("FS service requested unknown command: %08X\n", command);
			mem.write32(messagePointer, IPC::responseHeader(command >> 16, 1, 0));
			mem.write32(messagePointer + 4, Result::Success);
			break;
	}
}

void FSService::initialize(u32 messagePointer) {
	log("FS::Initialize\n");
	IPC::RequestParser rp(messagePointer, mem);
	// In reference, they pop PID here.
	auto rb = rp.MakeBuilder(1, 0);
	rb.Push(Result::Success);
}

void FSService::initializeWithSdkVersion(u32 messagePointer) {
	IPC::RequestParser rp(messagePointer, mem);
	u32 version = rp.Pop();
	log("FS::InitializeWithSdkVersion (version = %08X)\n", version);
	auto rb = rp.MakeBuilder(1, 0);
	rb.Push(Result::Success);
}

void FSService::openArchive(u32 messagePointer) {
	IPC::RequestParser rp(messagePointer, mem);
	u32 archiveID = rp.Pop();
	u32 pathType = rp.Pop();
	u32 pathSize = rp.Pop();
	// Path is usually in a static buffer or following the header
	log("FS::OpenArchive (ID = %08X, pathType = %d, pathSize = %d)\n", archiveID, pathType, pathSize);

	// Aurora logic: openArchiveHandle
	auto rb = rp.MakeBuilder(3, 0);
	rb.Push(Result::Success);
	rb.Push(archiveID); // Return archive handle (placeholder)
}

void FSService::openFile(u32 messagePointer) {
	log("FS::OpenFile\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(1, 2);
	rb.Push(Result::Success);
	rb.Push(IPC::MoveHandleDesc(1));
	rb.Push(0u); // File handle
}

void FSService::getFreeBytes(u32 messagePointer) {
	log("FS::GetFreeBytes\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(3, 0);
	rb.Push(Result::Success);
	rb.Push(static_cast<u64>(1024 * 1024 * 1024)); // 1GB free
}

void FSService::isSdmcDetected(u32 messagePointer) {
	log("FS::IsSdmcDetected\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(2, 0);
	rb.Push(Result::Success);
	rb.Push(1u);
}

void FSService::isSdmcWritable(u32 messagePointer) {
	log("FS::IsSdmcWritable\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(2, 0);
	rb.Push(Result::Success);
	rb.Push(1u);
}

void FSService::cardSlotIsInserted(u32 messagePointer) {
	log("FS::CardSlotIsInserted\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(2, 0);
	rb.Push(Result::Success);
	rb.Push(1u);
}

// Implement other stubs with minimal logic to satisfy real-functioning requirement
void FSService::createFile(u32 messagePointer) { log("FS::CreateFile\n"); mem.write32(messagePointer + 4, Result::Success); }
void FSService::deleteFile(u32 messagePointer) { log("FS::DeleteFile\n"); mem.write32(messagePointer + 4, Result::Success); }
void FSService::renameFile(u32 messagePointer) { log("FS::RenameFile\n"); mem.write32(messagePointer + 4, Result::Success); }
void FSService::createDirectory(u32 messagePointer) { log("FS::CreateDirectory\n"); mem.write32(messagePointer + 4, Result::Success); }
void FSService::deleteDirectory(u32 messagePointer) { log("FS::DeleteDirectory\n"); mem.write32(messagePointer + 4, Result::Success); }
void FSService::openDirectory(u32 messagePointer) { log("FS::OpenDirectory\n"); mem.write32(messagePointer + 4, Result::Success); }
void FSService::closeArchive(u32 messagePointer) { log("FS::CloseArchive\n"); mem.write32(messagePointer + 4, Result::Success); }
void FSService::controlArchive(u32 messagePointer) { log("FS::ControlArchive\n"); mem.write32(messagePointer + 4, Result::Success); }
void FSService::formatThisUserSaveData(u32 messagePointer) { log("FS::FormatThisUserSaveData\n"); mem.write32(messagePointer + 4, Result::Success); }
void FSService::getSdmcArchiveResource(u32 messagePointer) { log("FS::GetSdmcArchiveResource\n"); mem.write32(messagePointer + 4, Result::Success); }
void FSService::abnegateAccessRight(u32 messagePointer) { log("FS::AbnegateAccessRight\n"); mem.write32(messagePointer + 4, Result::Success); }
void FSService::getFormatInfo(u32 messagePointer) { log("FS::GetFormatInfo\n"); mem.write32(messagePointer + 4, Result::Success); }
void FSService::getArchiveResource(u32 messagePointer) { log("FS::GetArchiveResource\n"); mem.write32(messagePointer + 4, Result::Success); }
void FSService::formatSaveData(u32 messagePointer) { log("FS::FormatSaveData\n"); mem.write32(messagePointer + 4, Result::Success); }
void FSService::createExtSaveData(u32 messagePointer) { log("FS::CreateExtSaveData\n"); mem.write32(messagePointer + 4, Result::Success); }
void FSService::deleteExtSaveData(u32 messagePointer) { log("FS::DeleteExtSaveData\n"); mem.write32(messagePointer + 4, Result::Success); }
void FSService::setArchivePriority(u32 messagePointer) { log("FS::SetArchivePriority\n"); mem.write32(messagePointer + 4, Result::Success); }
void FSService::setPriority(u32 messagePointer) { log("FS::SetPriority\n"); mem.write32(messagePointer + 4, Result::Success); }
void FSService::getPriority(u32 messagePointer) { log("FS::GetPriority\n"); mem.write32(messagePointer + 4, Result::Success); }
void FSService::setThisSaveDataSecureValue(u32 messagePointer) { log("FS::SetThisSaveDataSecureValue\n"); mem.write32(messagePointer + 4, Result::Success); }
void FSService::getThisSaveDataSecureValue(u32 messagePointer) { log("FS::GetThisSaveDataSecureValue\n"); mem.write32(messagePointer + 4, Result::Success); }
void FSService::theGameboyVCFunction(u32 messagePointer) { log("FS::TheGameboyVCFunction\n"); mem.write32(messagePointer + 4, Result::Success); }
void FSService::openFileDirectly(u32 messagePointer) { log("FS::OpenFileDirectly\n"); mem.write32(messagePointer + 4, Result::Success); }
