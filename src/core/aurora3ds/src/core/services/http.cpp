#include "../../../include/services/http.hpp"

#include <algorithm>

#include "../../../include/ipc.hpp"
#include "../../../include/result/result.hpp"

namespace HTTPCommands {
	enum : u32 {
		Initialize = 0x00010044,
		CreateContext = 0x00020082,
		CloseContext = 0x00030040,
		BeginRequest = 0x00090040,
		GetResponseStatusCode = 0x00160000,
		ReceiveData = 0x001700C2,
		CreateRootCertChain = 0x002D0000,
		RootCertChainAddDefaultCert = 0x00300080,
	};
}

void HTTPService::reset() {
	initialized = false;
	nextRootCertChainHandle = 1;
	nextContextHandle = 1;
	rootCertChains.clear();
	contexts.clear();
}

void HTTPService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case HTTPCommands::BeginRequest: beginRequest(messagePointer); break;
		case HTTPCommands::CloseContext: closeContext(messagePointer); break;
		case HTTPCommands::CreateContext: createContext(messagePointer); break;
		case HTTPCommands::CreateRootCertChain: createRootCertChain(messagePointer); break;
		case HTTPCommands::GetResponseStatusCode: getResponseStatusCode(messagePointer); break;
		case HTTPCommands::Initialize: initialize(messagePointer); break;
		case HTTPCommands::ReceiveData: receiveData(messagePointer); break;
		case HTTPCommands::RootCertChainAddDefaultCert: rootCertChainAddDefaultCert(messagePointer); break;

		default:
			Helpers::warn("HTTP service requested. Command: %08X\n", command);
			mem.write32(messagePointer, IPC::responseHeader(command >> 16, 1, 0));
			mem.write32(messagePointer + 4, Result::FailurePlaceholder);
			break;
	}
}

void HTTPService::createContext(u32 messagePointer) {
	const u32 method = mem.read32(messagePointer + 4);
	const u32 urlSize = mem.read32(messagePointer + 8);
	const u32 urlPtr = mem.read32(messagePointer + 16);
	std::string url = mem.readString(urlPtr, urlSize);
	log("HTTP::CreateContext(method=%u, url=%s)\n", method, url.c_str());

	mem.write32(messagePointer, IPC::responseHeader(0x2, 2, 0));
	if (!initialized) {
		mem.write32(messagePointer + 4, Result::FailurePlaceholder);
		mem.write32(messagePointer + 8, 0);
		return;
	}

	const u32 handle = nextContextHandle++;
	HTTPContext ctx;
	ctx.method = method;
	ctx.url = std::move(url);
	contexts.emplace(handle, std::move(ctx));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, handle);
}

void HTTPService::closeContext(u32 messagePointer) {
	const u32 contextHandle = mem.read32(messagePointer + 4);
	log("HTTP::CloseContext(handle=%u)\n", contextHandle);
	contexts.erase(contextHandle);
	mem.write32(messagePointer, IPC::responseHeader(0x3, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void HTTPService::beginRequest(u32 messagePointer) {
	const u32 contextHandle = mem.read32(messagePointer + 4);
	log("HTTP::BeginRequest(handle=%u)\n", contextHandle);
	mem.write32(messagePointer, IPC::responseHeader(0x9, 1, 0));

	const auto it = contexts.find(contextHandle);
	if (it == contexts.end()) {
		mem.write32(messagePointer + 4, Result::FailurePlaceholder);
		return;
	}

	auto& ctx = it->second;
	ctx.requestStarted = true;
	ctx.readOffset = 0;
	ctx.statusCode = 200;
	// deterministic response payload
	const std::string payload = "AuroraHTTP";
	ctx.responseData.assign(payload.begin(), payload.end());
	mem.write32(messagePointer + 4, Result::Success);
}

void HTTPService::getResponseStatusCode(u32 messagePointer) {
	const u32 contextHandle = mem.read32(messagePointer + 4);
	log("HTTP::GetResponseStatusCode(handle=%u)\n", contextHandle);
	mem.write32(messagePointer, IPC::responseHeader(0x16, 2, 0));
	const auto it = contexts.find(contextHandle);
	if (it == contexts.end()) {
		mem.write32(messagePointer + 4, Result::FailurePlaceholder);
		mem.write32(messagePointer + 8, 0);
		return;
	}
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, it->second.statusCode);
}

void HTTPService::receiveData(u32 messagePointer) {
	const u32 contextHandle = mem.read32(messagePointer + 4);
	const u32 size = mem.read32(messagePointer + 8);
	const u32 outPtr = mem.read32(messagePointer + 16);
	log("HTTP::ReceiveData(handle=%u, size=%u, out=%08X)\n", contextHandle, size, outPtr);
	mem.write32(messagePointer, IPC::responseHeader(0x17, 2, 2));

	const auto it = contexts.find(contextHandle);
	if (it == contexts.end()) {
		mem.write32(messagePointer + 4, Result::FailurePlaceholder);
		mem.write32(messagePointer + 8, 0);
		return;
	}

	auto& ctx = it->second;
	const u32 remaining = static_cast<u32>(ctx.responseData.size()) - std::min<u32>(ctx.readOffset, ctx.responseData.size());
	const u32 readSize = std::min<u32>(size, remaining);
	for (u32 i = 0; i < readSize; i++) {
		mem.write8(outPtr + i, ctx.responseData[ctx.readOffset + i]);
	}
	ctx.readOffset += readSize;

	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, readSize);
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
