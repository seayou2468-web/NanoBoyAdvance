#include "../../../include/services/http.hpp"

#include "../../../include/ipc.hpp"
#include "../../../include/result/result.hpp"

namespace HTTPCommands {
	enum : u32 {
		Initialize = 0x00010044,
		CreateRootCertChain = 0x002D0000,
		RootCertChainAddDefaultCert = 0x00300080,
	};
}

void HTTPService::reset() {
	initialized = false;
	nextRootCertChainHandle = 1;
	rootCertChains.clear();
}

void HTTPService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case HTTPCommands::CreateRootCertChain: createRootCertChain(messagePointer); break;
		case HTTPCommands::Initialize: initialize(messagePointer); break;
		case HTTPCommands::RootCertChainAddDefaultCert: rootCertChainAddDefaultCert(messagePointer); break;

		default:
			Helpers::warn("HTTP service requested. Command: %08X\n", command);
			mem.write32(messagePointer + 4, Result::FailurePlaceholder);
			break;
	}
}

void HTTPService::initialize(u32 messagePointer) {
	const u32 postBufferSize = mem.read32(messagePointer + 4);
	const u32 postMemoryBlockHandle = mem.read32(messagePointer + 20);
	log("HTTP::Initialize (POST buffer size = %X, POST buffer memory block handle = %X)\n", postBufferSize, postMemoryBlockHandle);

	mem.write32(messagePointer, IPC::responseHeader(0x01, 1, 0));

	if (initialized) {
		Helpers::warn("HTTP: Tried to initialize service while already initialized");
		// TODO: Error code here
	}

	// 3DBrew: The provided POST buffer must be page-aligned (0x1000).
	if (postBufferSize & 0xfff) {
		Helpers::warn("HTTP: POST buffer size is not page-aligned");
	}

	initialized = true;
	// We currently don't emulate HTTP properly. TODO: Prepare POST buffer here
	mem.write32(messagePointer + 4, Result::Success);
}

void HTTPService::createRootCertChain(u32 messagePointer) {
	log("HTTP::CreateRootCertChain\n");
	mem.write32(messagePointer, IPC::responseHeader(0x2D, 2, 0));
	if (!initialized) {
		Helpers::warn("HTTP::CreateRootCertChain called before Initialize\n");
		mem.write32(messagePointer + 4, Result::FailurePlaceholder);
		mem.write32(messagePointer + 8, 0);
		return;
	}

	const u32 handle = nextRootCertChainHandle++;
	rootCertChains[handle] = {};

	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, handle);
}

void HTTPService::rootCertChainAddDefaultCert(u32 messagePointer) {
	log("HTTP::RootCertChainAddDefaultCert\n");
	const u32 contextHandle = mem.read32(messagePointer + 4);
	const u32 certID = mem.read32(messagePointer + 8);

	mem.write32(messagePointer, IPC::responseHeader(0x30, 2, 0));

	if (!initialized) {
		Helpers::warn("HTTP::RootCertChainAddDefaultCert called before Initialize\n");
		mem.write32(messagePointer + 4, Result::FailurePlaceholder);
		mem.write32(messagePointer + 8, 0);
		return;
	}

	auto it = rootCertChains.find(contextHandle);
	if (it == rootCertChains.end()) {
		Helpers::warn("HTTP::RootCertChainAddDefaultCert called with unknown handle %u\n", contextHandle);
		mem.write32(messagePointer + 4, Result::FailurePlaceholder);
		mem.write32(messagePointer + 8, 0);
		return;
	}

	// 3DBrew: 0-4 map to Nintendo root certs. Keep this bounded to default-cert domain.
	if (certID > 4) {
		Helpers::warn("HTTP::RootCertChainAddDefaultCert got invalid cert id %u\n", certID);
		mem.write32(messagePointer + 4, Result::FailurePlaceholder);
		mem.write32(messagePointer + 8, 0);
		return;
	}

	it->second.insert(certID);
	mem.write32(messagePointer + 4, Result::Success);
	// Return deterministic cert handle scoped to chain/cert.
	mem.write32(messagePointer + 8, (contextHandle << 8) | certID);
}
