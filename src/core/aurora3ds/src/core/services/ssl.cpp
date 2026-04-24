#include "../../../include/services/ssl.hpp"

#include <algorithm>

#include "../../../include/ipc.hpp"
#include "../../../include/result/result.hpp"

namespace SSLCommands {
	enum : u32 {
		Initialize = 0x0001,
		CreateContext = 0x0002,
		CreateRootCertChain = 0x0003,
		DestroyRootCertChain = 0x0004,
		AddTrustedRootCA = 0x0005,
		RootCertChainAddDefaultCert = 0x0006,
		RootCertChainRemoveCert = 0x0007,
		OpenClientCertContext = 0x000D,
		OpenDefaultClientCertContext = 0x000E,
		CloseClientCertContext = 0x000F,
		GenerateRandomData = 0x0011,
		InitializeConnectionSession = 0x0012,
		StartConnection = 0x0013,
		StartConnectionGetOut = 0x0014,
		Read = 0x0015,
		ReadPeek = 0x0016,
		Write = 0x0017,
		ContextSetRootCertChain = 0x0018,
		ContextSetClientCert = 0x0019,
		ContextClearOpt = 0x001B,
		ContextGetProtocolCipher = 0x001C,
		DestroyContext = 0x001E,
		ContextInitSharedmem = 0x001F,
	};
}

void SSLService::reset() {
	initialized = false;
	contexts.clear();
	rootChains.clear();
	clientCerts.clear();
	nextContextID = 1;
	nextRootChainID = 1;
	nextClientCertID = 1;

	// Use the default seed on reset to avoid funny bugs
	rng.seed();
}

void SSLService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	const u32 commandID = command >> 16;
	switch (commandID) {
		case SSLCommands::Initialize: initialize(messagePointer); break;
		case SSLCommands::CreateContext: createContext(messagePointer); break;
		case SSLCommands::CreateRootCertChain: createRootCertChain(messagePointer); break;
		case SSLCommands::DestroyRootCertChain: destroyRootCertChain(messagePointer); break;
		case SSLCommands::AddTrustedRootCA:
		case SSLCommands::RootCertChainAddDefaultCert:
		case SSLCommands::RootCertChainRemoveCert: acknowledgeChainOp(messagePointer, commandID); break;
		case SSLCommands::OpenClientCertContext: openClientCertContext(messagePointer); break;
		case SSLCommands::OpenDefaultClientCertContext: openDefaultClientCertContext(messagePointer); break;
		case SSLCommands::CloseClientCertContext: closeClientCertContext(messagePointer); break;
		case SSLCommands::GenerateRandomData: generateRandomData(messagePointer); break;
		case SSLCommands::InitializeConnectionSession: initializeConnectionSession(messagePointer); break;
		case SSLCommands::StartConnection: startConnection(messagePointer, false); break;
		case SSLCommands::StartConnectionGetOut: startConnection(messagePointer, true); break;
		case SSLCommands::Read: read(messagePointer, false); break;
		case SSLCommands::ReadPeek: read(messagePointer, true); break;
		case SSLCommands::Write: write(messagePointer); break;
		case SSLCommands::ContextSetRootCertChain: contextSetRootCertChain(messagePointer); break;
		case SSLCommands::ContextSetClientCert: contextSetClientCert(messagePointer); break;
		case SSLCommands::ContextClearOpt: contextClearOpt(messagePointer); break;
		case SSLCommands::ContextGetProtocolCipher: contextGetProtocolCipher(messagePointer); break;
		case SSLCommands::DestroyContext: destroyContext(messagePointer); break;
		case SSLCommands::ContextInitSharedmem: contextInitSharedmem(messagePointer); break;
		default: Helpers::panic("SSL service requested. Command: %08X\n", command);
	}
}

void SSLService::initialize(u32 messagePointer) {
	log("SSL::Initialize\n");
	mem.write32(messagePointer, IPC::responseHeader(0x01, 1, 0));

	if (initialized) {
		Helpers::warn("SSL service initialized twice");
	}

	initialized = true;
	rng.seed(std::random_device()());  // Seed rng via std::random_device

	mem.write32(messagePointer + 4, Result::Success);
}

void SSLService::createContext(u32 messagePointer) {
	const u32 contextID = nextContextID++;
	contexts.emplace(contextID, Context{});
	mem.write32(messagePointer, IPC::responseHeader(0x02, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, contextID);
}

void SSLService::createRootCertChain(u32 messagePointer) {
	const u32 chainID = nextRootChainID++;
	rootChains.emplace(chainID);
	mem.write32(messagePointer, IPC::responseHeader(0x03, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, chainID);
}

void SSLService::destroyRootCertChain(u32 messagePointer) {
	const u32 chainID = mem.read32(messagePointer + 4);
	rootChains.erase(chainID);
	mem.write32(messagePointer, IPC::responseHeader(0x04, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void SSLService::acknowledgeChainOp(u32 messagePointer, u32 commandID) {
	mem.write32(messagePointer, IPC::responseHeader(commandID, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void SSLService::openClientCertContext(u32 messagePointer) {
	const u32 certID = nextClientCertID++;
	clientCerts.emplace(certID);
	mem.write32(messagePointer, IPC::responseHeader(0x0D, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, certID);
}

void SSLService::openDefaultClientCertContext(u32 messagePointer) {
	const u32 certID = nextClientCertID++;
	clientCerts.emplace(certID);
	mem.write32(messagePointer, IPC::responseHeader(0x0E, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, certID);
}

void SSLService::closeClientCertContext(u32 messagePointer) {
	clientCerts.erase(mem.read32(messagePointer + 4));
	mem.write32(messagePointer, IPC::responseHeader(0x0F, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void SSLService::generateRandomData(u32 messagePointer) {
	const u32 size = mem.read32(messagePointer + 4);
	const u32 output = mem.read32(messagePointer + 12);
	log("SSL::GenerateRandomData (out = %08X, size = %08X)\n", output, size);

	// TODO: This might be a biiit slow, might want to make it write in word quantities
	u32 data;

	for (u32 i = 0; i < size; i++) {
		// We don't have an available random value since we're on a multiple of 4 bytes and our Twister is 32-bit, generate a new one from the
		// Mersenne Twister
		if ((i & 3) == 0) {
			data = rng();
		}

		mem.write8(output + i, u8(data));
		// Shift data by 8 to get the next byte
		data >>= 8;
	}

	mem.write32(messagePointer, IPC::responseHeader(0x11, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
}

void SSLService::initializeConnectionSession(u32 messagePointer) {
	const u32 contextID = mem.read32(messagePointer + 4);
	contexts.emplace(contextID, Context{});
	mem.write32(messagePointer, IPC::responseHeader(0x12, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void SSLService::startConnection(u32 messagePointer, bool withOut) {
	const u32 contextID = mem.read32(messagePointer + 4);
	if (auto it = contexts.find(contextID); it != contexts.end()) {
		it->second.started = true;
	}

	mem.write32(messagePointer, IPC::responseHeader(withOut ? 0x14 : 0x13, withOut ? 2 : 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
	if (withOut) mem.write32(messagePointer + 8, 0);
}

void SSLService::read(u32 messagePointer, bool peek) {
	const u32 contextID = mem.read32(messagePointer + 4);
	const u32 size = mem.read32(messagePointer + 8);
	const u32 out = mem.read32(messagePointer + 16);

	u32 bytesRead = 0;
	if (auto it = contexts.find(contextID); it != contexts.end()) {
		const auto& src = it->second.ioBuffer;
		bytesRead = std::min<u32>(size, static_cast<u32>(src.size()));
		for (u32 i = 0; i < bytesRead; i++) {
			mem.write8(out + i, src[i]);
		}
		if (!peek) {
			it->second.ioBuffer.erase(it->second.ioBuffer.begin(), it->second.ioBuffer.begin() + bytesRead);
		}
	}

	mem.write32(messagePointer, IPC::responseHeader(peek ? 0x16 : 0x15, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, bytesRead);
}

void SSLService::write(u32 messagePointer) {
	const u32 contextID = mem.read32(messagePointer + 4);
	const u32 size = mem.read32(messagePointer + 8);
	const u32 in = mem.read32(messagePointer + 16);

	auto& buffer = contexts[contextID].ioBuffer;
	buffer.reserve(buffer.size() + size);
	for (u32 i = 0; i < size; i++) {
		buffer.push_back(mem.read8(in + i));
	}

	mem.write32(messagePointer, IPC::responseHeader(0x17, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, size);
}

void SSLService::contextSetRootCertChain(u32 messagePointer) {
	const u32 contextID = mem.read32(messagePointer + 4);
	const u32 chainID = mem.read32(messagePointer + 8);
	if (auto it = contexts.find(contextID); it != contexts.end() && rootChains.contains(chainID)) {
		it->second.rootChainID = chainID;
	}
	mem.write32(messagePointer, IPC::responseHeader(0x18, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void SSLService::contextSetClientCert(u32 messagePointer) {
	const u32 contextID = mem.read32(messagePointer + 4);
	const u32 certID = mem.read32(messagePointer + 8);
	if (auto it = contexts.find(contextID); it != contexts.end() && clientCerts.contains(certID)) {
		it->second.clientCertID = certID;
	}
	mem.write32(messagePointer, IPC::responseHeader(0x19, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void SSLService::contextClearOpt(u32 messagePointer) {
	const u32 contextID = mem.read32(messagePointer + 4);
	const u32 mask = mem.read32(messagePointer + 8);
	if (auto it = contexts.find(contextID); it != contexts.end()) {
		it->second.options &= ~mask;
	}
	mem.write32(messagePointer, IPC::responseHeader(0x1B, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void SSLService::contextGetProtocolCipher(u32 messagePointer) {
	mem.write32(messagePointer, IPC::responseHeader(0x1C, 3, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0x0303);  // TLS 1.2
	mem.write32(messagePointer + 12, 0x002F); // TLS_RSA_WITH_AES_128_CBC_SHA
}

void SSLService::destroyContext(u32 messagePointer) {
	contexts.erase(mem.read32(messagePointer + 4));
	mem.write32(messagePointer, IPC::responseHeader(0x1E, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void SSLService::contextInitSharedmem(u32 messagePointer) {
	mem.write32(messagePointer, IPC::responseHeader(0x1F, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}
