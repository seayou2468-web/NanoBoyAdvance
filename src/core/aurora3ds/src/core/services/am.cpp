#include "../../../include/services/am.hpp"

#include "../../../include/ipc.hpp"

namespace AMCommands {
	enum : u32 {
		GetNumPrograms = 0x00010040,
		GetProgramList = 0x00020082,
		GetProgramInfos = 0x00030084,
		DeleteUserProgram = 0x00040040,
		GetProductCode = 0x00050042,
		GetStorageId = 0x00060000,
		DeleteTicket = 0x00090040,
		GetNumTickets = 0x000A0040,
		GetTicketList = 0x000B0082,
		NeedsCleanup = 0x000C0000,
		DoCleanup = 0x000D0000,
		GetDeviceID = 0x000E0000,
		GetNumImportTitleContexts = 0x000F0000,
		GetImportTitleContextList = 0x00100042,
		GetImportTitleContexts = 0x00110082,
		DeleteImportTitleContext = 0x00120040,
		GetNumImportContentContexts = 0x00130000,
		GetImportContentContextList = 0x00140042,
		GetImportContentContexts = 0x00150082,
		GetNumCurrentImportTitleContexts = 0x00170000,
		GetCurrentImportTitleContextList = 0x00180042,
		GetCurrentImportTitleContexts = 0x00190082,
		GetNumCurrentImportContentContexts = 0x001A0000,
		GetCurrentImportContentContextList = 0x001B0042,
		GetCurrentImportContentContexts = 0x001C0082,
		GetDLCTitleInfo = 0x10050084,
		ListTitleInfo = 0x10070102,
		GetPatchTitleInfo = 0x100D0084,
		BeginImportProgram = 0x04020040,
		BeginImportProgramTemporarily = 0x04030000,
		CancelImportProgram = 0x04040002,
		EndImportProgram = 0x04050002,
		EndImportProgramWithoutCommit = 0x04060002,
		CommitImportPrograms = 0x040700C2,
		GetProgramInfoFromCia = 0x04080042,
		GetSystemMenuDataFromCia = 0x04090044,
		GetDependencyListFromCia = 0x040A0044,
		GetTransferSizeFromCia = 0x040B0002,
		GetCoreVersionFromCia = 0x040C0002,
		GetRequiredSizeFromCia = 0x040D0042,
		CommitImportProgramsAndUpdateFirmwareAuto = 0x040E0000,
		DeleteProgram = 0x041000C0,
		GetSystemUpdaterMutex = 0x04120000,
		GetMetaSizeFromCia = 0x04130002,
		GetMetaDataFromCia = 0x04140044,
		BeginImportTicket = 0x04150040,
		EndImportTicket = 0x04160002,
		BeginImportTitle = 0x04170040,
		StopImportTitle = 0x04180000,
		ResumeImportTitle = 0x04190000,
		CancelImportTitle = 0x041A0000,
		EndImportTitle = 0x041B0000,
		CommitImportTitles = 0x041C0000,
		BeginImportTmd = 0x041D0000,
		EndImportTmd = 0x041E0000,
		CreateImportContentContexts = 0x041F0000,
		BeginImportContent = 0x04200080,
		ResumeImportContent = 0x04210080,
		StopImportContent = 0x04220000,
		CancelImportContent = 0x04230000,
		EndImportContent = 0x04240000,
		Sign = 0x04280042,
		GetDeviceCert = 0x04290082,
		CommitImportTitlesAndUpdateFirmwareAuto = 0x042A0000,
		DeleteTicketId = 0x042B0040,
		GetNumTicketIds = 0x042C0040,
		GetTicketIdList = 0x042D0082,
		GetNumTicketsOfProgram = 0x042E0080,
		ListTicketInfos = 0x042F00C2,
		GetNumCurrentContentInfos = 0x04300000,
		FindCurrentContentInfos = 0x04310082,
		ListCurrentContentInfos = 0x043200C2,
		CalculateContextRequiredSize = 0x04330040,
		UpdateImportContentContexts = 0x04340040,
		ExportTicketWrapped = 0x043500C2,
	};
}

void AMService::reset() {}

static void writeResult(Memory& mem, u32 messagePointer, u32 commandID, u32 words) {
	mem.write32(messagePointer, IPC::responseHeader(commandID, words, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void AMService::respondNotImplemented(u32 messagePointer, u32 command) {
	const u32 commandID = command >> 16;
	log("AM command %04X [stubbed: NotImplemented]\n", commandID);
	mem.write32(messagePointer, IPC::responseHeader(commandID, 1, 0));
	mem.write32(messagePointer + 4, Result::OS::NotImplemented);
}

void AMService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	const u32 commandID = command >> 16;

	switch (command) {
		case AMCommands::GetPatchTitleInfo: getPatchTitleInfo(messagePointer); return;
		case AMCommands::GetDLCTitleInfo: getDLCTitleInfo(messagePointer); return;
		case AMCommands::ListTitleInfo: listTitleInfo(messagePointer); return;

		case AMCommands::GetNumPrograms:
		case AMCommands::GetNumTickets:
		case AMCommands::GetNumImportTitleContexts:
		case AMCommands::GetNumImportContentContexts:
		case AMCommands::GetNumCurrentImportTitleContexts:
		case AMCommands::GetNumCurrentImportContentContexts:
		case AMCommands::GetNumTicketIds:
		case AMCommands::GetNumTicketsOfProgram:
		case AMCommands::GetNumCurrentContentInfos:
			writeResult(mem, messagePointer, commandID, 2);
			mem.write32(messagePointer + 8, 0);
			return;

		case AMCommands::GetStorageId:
			writeResult(mem, messagePointer, commandID, 2);
			mem.write32(messagePointer + 8, 0);
			return;

		case AMCommands::NeedsCleanup:
			writeResult(mem, messagePointer, commandID, 2);
			mem.write8(messagePointer + 8, 0);
			return;

		case AMCommands::GetDeviceID:
			writeResult(mem, messagePointer, commandID, 3);
			mem.write64(messagePointer + 8, 0x0000000100000000ULL);
			return;

		case AMCommands::GetProductCode:
			writeResult(mem, messagePointer, commandID, 4);
			mem.write64(messagePointer + 8, 0);
			mem.write64(messagePointer + 16, 0);
			return;

		case AMCommands::GetProgramList:
		case AMCommands::GetProgramInfos:
		case AMCommands::GetTicketList:
		case AMCommands::GetImportTitleContextList:
		case AMCommands::GetImportTitleContexts:
		case AMCommands::GetImportContentContextList:
		case AMCommands::GetImportContentContexts:
		case AMCommands::GetCurrentImportTitleContextList:
		case AMCommands::GetCurrentImportTitleContexts:
		case AMCommands::GetCurrentImportContentContextList:
		case AMCommands::GetCurrentImportContentContexts:
		case AMCommands::GetTicketIdList:
		case AMCommands::ListTicketInfos:
		case AMCommands::FindCurrentContentInfos:
		case AMCommands::ListCurrentContentInfos:
			writeResult(mem, messagePointer, commandID, 2);
			mem.write32(messagePointer + 8, 0);
			return;

		case AMCommands::DeleteUserProgram:
		case AMCommands::DeleteTicket:
		case AMCommands::DoCleanup:
		case AMCommands::DeleteImportTitleContext:
		case AMCommands::DeleteProgram:
		case AMCommands::CommitImportPrograms:
		case AMCommands::CommitImportProgramsAndUpdateFirmwareAuto:
		case AMCommands::BeginImportTicket:
		case AMCommands::EndImportTicket:
		case AMCommands::BeginImportTitle:
		case AMCommands::StopImportTitle:
		case AMCommands::ResumeImportTitle:
		case AMCommands::CancelImportTitle:
		case AMCommands::EndImportTitle:
		case AMCommands::CommitImportTitles:
		case AMCommands::BeginImportTmd:
		case AMCommands::EndImportTmd:
		case AMCommands::CreateImportContentContexts:
		case AMCommands::BeginImportContent:
		case AMCommands::ResumeImportContent:
		case AMCommands::StopImportContent:
		case AMCommands::CancelImportContent:
		case AMCommands::EndImportContent:
		case AMCommands::CommitImportTitlesAndUpdateFirmwareAuto:
		case AMCommands::DeleteTicketId:
		case AMCommands::UpdateImportContentContexts:
			writeResult(mem, messagePointer, commandID, 1);
			return;

		case AMCommands::CalculateContextRequiredSize:
			writeResult(mem, messagePointer, commandID, 2);
			mem.write32(messagePointer + 8, 0x1000);
			return;

		// Large-scale integration path for CIA/sign/cert/meta commands.
		case AMCommands::BeginImportProgram:
		case AMCommands::BeginImportProgramTemporarily:
			writeResult(mem, messagePointer, commandID, 2);
			mem.write32(messagePointer + 8, 0);  // Placeholder import handle
			return;

		case AMCommands::CancelImportProgram:
		case AMCommands::EndImportProgram:
		case AMCommands::EndImportProgramWithoutCommit:
		case AMCommands::GetSystemMenuDataFromCia:
		case AMCommands::GetDependencyListFromCia:
		case AMCommands::GetMetaDataFromCia:
		case AMCommands::GetSystemUpdaterMutex:
		case AMCommands::Sign:
		case AMCommands::GetDeviceCert:
		case AMCommands::ExportTicketWrapped:
			writeResult(mem, messagePointer, commandID, 1);
			return;

		case AMCommands::GetProgramInfoFromCia:
			writeResult(mem, messagePointer, commandID, 8);
			for (u32 i = 0; i < 7; i++) {
				mem.write32(messagePointer + 8 + i * 4, 0);
			}
			return;

		case AMCommands::GetTransferSizeFromCia:
		case AMCommands::GetRequiredSizeFromCia:
			writeResult(mem, messagePointer, commandID, 3);
			mem.write64(messagePointer + 8, 0);
			return;

		case AMCommands::GetCoreVersionFromCia:
		case AMCommands::GetMetaSizeFromCia:
			writeResult(mem, messagePointer, commandID, 2);
			mem.write32(messagePointer + 8, 0);
			return;

		default:
			respondNotImplemented(messagePointer, command);
			return;
	}
}

void AMService::listTitleInfo(u32 messagePointer) {
	log("AM::ListDLCOrLicenseTicketInfos\n");  // Yes this is the actual name
	u32 ticketCount = mem.read32(messagePointer + 4);
	u64 titleID = mem.read64(messagePointer + 8);
	u32 pointer = mem.read32(messagePointer + 24);

	for (u32 i = 0; i < ticketCount; i++) {
		mem.write64(pointer, titleID);  // Title ID
		mem.write64(pointer + 8, 0);    // Ticket ID
		mem.write16(pointer + 16, 0);   // Version
		mem.write16(pointer + 18, 0);   // Padding
		mem.write32(pointer + 20, 0);   // Size

		pointer += 24;  // = sizeof(TicketInfo)
	}

	mem.write32(messagePointer, IPC::responseHeader(0x1007, 2, 2));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, ticketCount);
}

void AMService::getDLCTitleInfo(u32 messagePointer) {
	log("AM::GetDLCTitleInfo (integrated empty response)\n");
	mem.write32(messagePointer, IPC::responseHeader(0x1005, 2, 2));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0);
}

void AMService::getPatchTitleInfo(u32 messagePointer) {
	log("AM::GetPatchTitleInfo (integrated empty response)\n");
	mem.write32(messagePointer, IPC::responseHeader(0x100D, 2, 2));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0);
}
