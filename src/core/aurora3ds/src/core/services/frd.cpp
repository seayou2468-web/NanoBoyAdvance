#include "../../../include/services/frd.hpp"
#include "../../../include/ipc.hpp"
#include <algorithm>

namespace FRDCommands {
	enum : u32 {
		HasLoggedIn = 0x00010000,
		IsOnline = 0x00020000,
		Logout = 0x00040000,
		GetMyFriendKey = 0x00050000,
		GetMyProfile = 0x00070000,
		GetMyPresence = 0x00080000,
		GetMyScreenName = 0x00090000,
		GetMyMii = 0x000A0000,
		GetMyFavoriteGame = 0x000D0000,
		GetMyComment = 0x000F0000,
		GetFriendKeyList = 0x00110042,
		GetFriendPresence = 0x00120042,
		GetFriendProfile = 0x00150042,
		GetFriendRelationship = 0x00160042,
		GetFriendAttributeFlags = 0x00170042,
		SetNotificationMask = 0x00210040,
		SetClientSdkVersion = 0x00320042,
		UpdateGameModeDescription = 0x001D0002,
		SaveLocalAccountData = 0x04050000,
		UpdateMii = 0x040C0800,
	};
}

void FRDService::reset() { loggedIn = true; }

void FRDService::handleSyncRequest(u32 messagePointer, FRDService::Type type) {
	const u32 command = mem.read32(messagePointer);
	IPC::RequestParser rp(messagePointer, mem);
	switch (command) {
		case FRDCommands::HasLoggedIn: {
			auto rb = rp.MakeBuilder(2, 0);
			rb.Push(Result::Success);
			rb.Push(loggedIn ? 1u : 0u);
			break;
		}
		case FRDCommands::IsOnline: {
			auto rb = rp.MakeBuilder(2, 0);
			rb.Push(Result::Success);
			rb.Push(1u);
			break;
		}
		case FRDCommands::GetMyScreenName: {
			auto rb = rp.MakeBuilder(1, 0);
			rb.Push(Result::Success);
			break;
		}
		default:
			log("FRD service requested unknown command: %08X\n", command);
			auto rb = rp.MakeBuilder(1, 0);
			rb.Push(Result::Success);
			break;
	}
}

// Minimal stubs to satisfy compiler/linking if any
void FRDService::attachToEventNotification(u32 messagePointer) {}
void FRDService::getFriendAttributeFlags(u32 messagePointer) {}
void FRDService::getFriendKeyList(u32 messagePointer) {}
void FRDService::getFriendPresence(u32 messagePointer) {}
void FRDService::getFriendProfile(u32 messagePointer) {}
void FRDService::getFriendRelationship(u32 messagePointer) {}
void FRDService::getMyComment(u32 messagePointer) {}
void FRDService::getMyFriendKey(u32 messagePointer) {}
void FRDService::getMyMii(u32 messagePointer) {}
void FRDService::getMyFavoriteGame(u32 messagePointer) {}
void FRDService::getMyPresence(u32 messagePointer) {}
void FRDService::getMyProfile(u32 messagePointer) {}
void FRDService::getMyScreenName(u32 messagePointer) {}
void FRDService::hasLoggedIn(u32 messagePointer) {}
void FRDService::isOnline(u32 messagePointer) {}
void FRDService::logout(u32 messagePointer) {}
void FRDService::setClientSDKVersion(u32 messagePointer) {}
void FRDService::setNotificationMask(u32 messagePointer) {}
void FRDService::updateGameModeDescription(u32 messagePointer) {}
void FRDService::saveLocalAccountData(u32 messagePointer) {}
void FRDService::updateMii(u32 messagePointer) {}
