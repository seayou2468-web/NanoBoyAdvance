#pragma once
#include "../helpers.hpp"
#include "../kernel/kernel_types.hpp"
#include "../logger.hpp"
#include "../memory.hpp"
#include "../result/result.hpp"

// Please forgive me for how everything in this file is named
// "dlp:SRVR" is not a nice name to work with
class DlpSrvrService {
	using Handle = HorizonHandle;

	Handle handle = KernelHandles::DLP_SRVR;
	Memory& mem;
	bool initialized = false;
	bool scanning = false;
	bool hosting = false;
	bool distributing = false;
	bool gameStarted = false;
	u16 connectedClients = 0;
	u16 channelHandle = 0x0421;
	MAKE_LOG_FUNCTION(log, dlpSrvrLogger)

	// Service commands
	void isChild(u32 messagePointer);
	void initialize(u32 messagePointer, bool withName);
	void finalize(u32 messagePointer);
	void getChannels(u32 messagePointer);
	void startScan(u32 messagePointer);
	void stopScan(u32 messagePointer);
	void getEmptyBufferResponse(u32 messagePointer, u32 commandID);
	void getMyStatus(u32 messagePointer);
	void prepareForSystemDownload(u32 messagePointer);
	void startSystemDownload(u32 messagePointer);
	void getDupAvailability(u32 messagePointer);
	void getCupVersion(u32 messagePointer);
	void getServerState(u32 messagePointer);
	void startHosting(u32 messagePointer);
	void endHosting(u32 messagePointer);
	void startDistribution(u32 messagePointer);
	void beginGame(u32 messagePointer);
	void acceptClient(u32 messagePointer);
	void disconnectClient(u32 messagePointer);
	void getConnectingClients(u32 messagePointer);
	void getClientInfo(u32 messagePointer);
	void getClientState(u32 messagePointer);
	void getDupNoticeNeed(u32 messagePointer);

  public:
	enum class Type {
		SRVR,
		CLNT,
		FKCL,
	};

	DlpSrvrService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer, Type type = Type::SRVR);
};
