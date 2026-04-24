#include "../../include/memory.hpp"

#include <cassert>
#include <chrono>  // For time since epoch
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "../../include/kernel/config_mem.hpp"
#include "../../include/kernel/fcram.hpp"
#include "../../include/kernel/resource_limits.hpp"
#include "../../include/services/fonts.hpp"
#include "../../include/services/ptm.hpp"
#include "../../include/platform/shared_font_bundle.hpp"

using namespace KernelMemoryTypes;

namespace {

std::optional<std::filesystem::path> g_sharedFontReplacementOverridePath;

std::vector<u8> LoadFile(const std::filesystem::path& path) {
	std::ifstream in(path, std::ios::binary);
	if (!in.good()) {
		return {};
	}

	return std::vector<u8>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

bool ShouldLogUnmappedMemoryAccess() {
	static int count = 0;
	if (count < 20) {
		count++;
		return true;
	}
	return false;
}

void LogUnmappedRead(u32 bits, u32 vaddr) {
	if (!ShouldLogUnmappedMemoryAccess()) {
		return;
	}

	Helpers::warn("Returning 0 from unmapped %u-bit read, addr: %08X", bits, vaddr);
}

void LogUnmappedWrite(u32 bits, u32 vaddr, u32 value) {
	if (!ShouldLogUnmappedMemoryAccess()) {
		return;
	}

	Helpers::warn("Ignoring unmapped %u-bit write, addr: %08X, val: %08X", bits, vaddr, value);
}

struct PhysicalRegionView {
	u8* hostPtr;
	size_t size;
};

std::optional<PhysicalRegionView> ResolvePhysicalRegion(u8* fcram, u8* dspRam, u8* vram, u32 paddr) {
	if (paddr >= PhysicalAddrs::FCRAM && paddr < PhysicalAddrs::FCRAM + Memory::FCRAM_SIZE) {
		const size_t offset = paddr - PhysicalAddrs::FCRAM;
		return PhysicalRegionView{
			.hostPtr = fcram + offset,
			.size = Memory::FCRAM_SIZE - offset,
		};
	}

	if (paddr >= PhysicalAddrs::DSP_RAM && paddr < PhysicalAddrs::DSP_RAM + Memory::DSP_RAM_SIZE) {
		const size_t offset = paddr - PhysicalAddrs::DSP_RAM;
		return PhysicalRegionView{
			.hostPtr = dspRam + offset,
			.size = Memory::DSP_RAM_SIZE - offset,
		};
	}

	if (paddr >= PhysicalAddrs::VRAM && paddr < PhysicalAddrs::VRAM + VirtualAddrs::VramSize) {
		if (vram == nullptr) {
			return std::nullopt;
		}
		const size_t offset = paddr - PhysicalAddrs::VRAM;
		return PhysicalRegionView{
			.hostPtr = vram + offset,
			.size = VirtualAddrs::VramSize - offset,
		};
	}

	return std::nullopt;
}

constexpr u32 kArmBranchSelf = 0xEAFFFFFE;      // b .
constexpr u32 kArmReturnPrefetchAbort = 0xE25EF004;  // subs pc, lr, #4
constexpr u32 kArmReturnDataAbort = 0xE25EF008;      // subs pc, lr, #8

std::vector<u8> LoadSharedFontReplacement() {
	if (g_sharedFontReplacementOverridePath.has_value()) {
		if (std::vector<u8> data = LoadFile(*g_sharedFontReplacementOverridePath); !data.empty()) {
			return data;
		}
	}

	// iOS/macOS app bundles first.
	if (const auto bundleFontPath = Platform::GetSharedFontReplacementBundlePath(); bundleFontPath.has_value()) {
		if (std::vector<u8> data = LoadFile(*bundleFontPath); !data.empty()) {
			return data;
		}
	}

	// Fallbacks for local/dev runs.
	if (std::vector<u8> data = LoadFile("SharedFontReplacement.bin"); !data.empty()) {
		return data;
	}
	if (std::vector<u8> data = LoadFile("Resources/fonts/SharedFontReplacement.bin"); !data.empty()) {
		return data;
	}

	return {};
}

}  // namespace

void SetSharedFontReplacementOverridePath(const std::filesystem::path& path) {
	if (path.empty()) {
		g_sharedFontReplacementOverridePath = std::nullopt;
		return;
	}
	g_sharedFontReplacementOverridePath = path;
}

void ClearSharedFontReplacementOverridePath() {
	g_sharedFontReplacementOverridePath = std::nullopt;
}

void* Memory::ensureProcessVM(u32 process_handle) {
	auto [it, inserted] = processAddressSpaces.try_emplace(process_handle);
	if (inserted) {
		it->second = vmManager;
	}
	return &it->second;
}

void Memory::activateProcessVM(u32 process_handle) {
	auto [it, inserted] = processAddressSpaces.try_emplace(process_handle);
	if (inserted) {
		it->second = vmManager;
	}
	activeVmManager = &it->second;
}

Memory::Memory(KFcram& fcramManager, const EmulatorConfig& config) : fcramManager(fcramManager), config(config) {
	const bool fastmemEnabled = config.fastmemEnabled;
	arena = new Common::HostMemory(FASTMEM_BACKING_SIZE, FASTMEM_VIRTUAL_SIZE, fastmemEnabled);

	vmManager.resize(totalPageCount);
	fcram = arena->BackingBasePointer() + FASTMEM_FCRAM_OFFSET;
	dspRam = arena->BackingBasePointer() + FASTMEM_DSP_RAM_OFFSET;
	useFastmem = fastmemEnabled && arena->VirtualBasePointer() != nullptr;
}

void Memory::reset() {
	// Mark the entire process address space as free
	constexpr static int MAX_USER_PAGES = 0x40000000 >> 12;
	memoryInfo.clear();
	memoryInfo.push_back(MemoryInfo(0, MAX_USER_PAGES, 0, KernelMemoryTypes::Free));

	// TODO: remove this, only needed to make the subsequent allocations work for now
	fcramManager.reset(FCRAM_SIZE, FCRAM_APPLICATION_SIZE, FCRAM_SYSTEM_SIZE, FCRAM_BASE_SIZE);

	if (useFastmem) {
		// Unmap any mappings when resetting
		arena->Unmap(0, 4_GB, false);
	}

	activeVM().clear();
	std::fill(exceptionVectorPage.begin(), exceptionVectorPage.end(), 0);
	auto* exceptionVectors = reinterpret_cast<u32*>(exceptionVectorPage.data());
	// ARM low vectors @ 0x00000000:
	// Reset/Undefined/SVC/Reserved/IRQ/FIQ loop forever unless explicitly handled.
	exceptionVectors[0x00 / sizeof(u32)] = kArmBranchSelf;
	exceptionVectors[0x04 / sizeof(u32)] = kArmBranchSelf;
	exceptionVectors[0x08 / sizeof(u32)] = kArmBranchSelf;
	exceptionVectors[0x14 / sizeof(u32)] = kArmBranchSelf;
	exceptionVectors[0x18 / sizeof(u32)] = kArmBranchSelf;
	exceptionVectors[0x1C / sizeof(u32)] = kArmBranchSelf;
	// Abort vectors return according to ARM semantics.
	exceptionVectors[0x0C / sizeof(u32)] = kArmReturnPrefetchAbort;
	exceptionVectors[0x10 / sizeof(u32)] = kArmReturnDataAbort;
	activeVM().readTable[0] = reinterpret_cast<uintptr_t>(exceptionVectorPage.data());
	// Match Citra/Azahar behavior for userland startup code that repopulates low vectors
	// during ROM initialization: keep the first page writable while it is mapped.
	activeVM().writeTable[0] = reinterpret_cast<uintptr_t>(exceptionVectorPage.data());
	activeVM().pageTableAttrs[0] = PageType::Memory;

	// Allocate 512 bytes of TLS for each thread. Since the smallest allocatable unit is 4 KB, that means allocating one page for every 8 threads
	// Note that TLS is always allocated in the Base region
	s32 tlsPages = (appResourceLimits.maxThreads + 7) >> 3;
	allocMemory(VirtualAddrs::TLSBase, tlsPages, FcramRegion::Base, true, true, false, MemoryState::Locked);

	// Initialize shared memory blocks and reserve memory for them
	for (auto& e : sharedMemBlocks) {
		if (e.handle == KernelHandles::FontSharedMemHandle) {
			// Font shared memory size depends on the external shared-font replacement file.
			e.size = LoadSharedFontReplacement().size();
		}

		e.mapped = false;
		FcramBlockList memBlock;
		fcramManager.alloc(memBlock, e.size >> 12, FcramRegion::Sys, false);
		e.paddr = memBlock.begin()->paddr;
	}

	// Map DSP RAM as R/W at [0x1FF00000, 0x1FF7FFFF]
	constexpr u32 dspRamPages = DSP_RAM_SIZE / pageSize;  // Number of DSP RAM pages

	u32 vaddr = VirtualAddrs::DSPMemStart;
	u32 paddr = PhysicalAddrs::DSP_RAM;

	Operation op{.newState = MemoryState::Static, .r = true, .w = true, .changeState = true, .changePerms = true};
	changeMemoryState(vaddr, dspRamPages, op);
	mapPhysicalMemory(vaddr, paddr, dspRamPages, true, true, false);

	// Allocate RW mapping for DSP RAM
	// addFastmemView(VirtualAddrs::DSPMemStart, FASTMEM_DSP_RAM_OFFSET, DSP_RAM_SIZE, true, false);

	// Later adjusted based on ROM header when possible
	region = Regions::USA;
	// Keep a canonical VM snapshot so newly created processes inherit baseline mappings.
	vmManager = activeVM();
}

bool Memory::allocateMainThreadStack(u32 size) {
	// Map stack pages as R/W
	// TODO: get the region from the exheader
	const u32 stackBottom = VirtualAddrs::StackTop - size;
	return allocMemory(stackBottom, size >> 12, FcramRegion::App, true, true, false, MemoryState::Locked);
}

u8 Memory::read8(u32 vaddr) {
	const u32 page = vaddr >> pageShift;
	const u32 offset = vaddr & pageMask;

	uintptr_t pointer = activeVM().readTable[page];
	if (pointer != 0) [[likely]] {
		return *(u8*)(pointer + offset);
	} else {
		if (u8* privileged = resolvePrivilegedVirtualPointer(vaddr); privileged != nullptr) {
			return *privileged;
		}
		switch (vaddr) {
			case ConfigMem::BatteryState: {
				// Set by the PTM module
				// Charger plugged: Shows whether the charger is plugged
				// Charging: Shows whether the charger is plugged and the console is actually charging, ie the battery is not full
				// BatteryLevel: A battery level calculated via PTM::GetBatteryLevel
				// These are all assembled into a bitfield and returned via config memory
				const bool chargerPlugged = config.chargerPlugged;
				const bool charging = config.chargerPlugged && (config.batteryPercentage < 100);
				const auto batteryLevel = static_cast<BatteryLevel>(PTMService::batteryPercentToLevel(config.batteryPercentage));

				return getBatteryState(chargerPlugged, charging, batteryLevel);
			}
			case ConfigMem::EnvInfo: return envInfo;
			case ConfigMem::HardwareType: return ConfigMem::HardwareCodes::Product;
			case ConfigMem::KernelVersionMinor: return u8(kernelVersion & 0xff);
			case ConfigMem::KernelVersionMajor: return u8(kernelVersion >> 8);
			case ConfigMem::LedState3D: return 1;    // Report the 3D LED as always off (non-zero) for now
			case ConfigMem::NetworkState: return 2;  // Report that we've got an internet connection
			case ConfigMem::HeadphonesConnectedMaybe: return 0;
			case ConfigMem::Unknown1086: return 1;  // It's unknown what this is but some games want it to be 1

			case ConfigMem::FirmUnknown: return firm.unk;
			case ConfigMem::FirmRevision: return firm.revision;
			case ConfigMem::FirmVersionMinor: return firm.minor;
			case ConfigMem::FirmVersionMajor: return firm.major;
			case ConfigMem::WifiLevel: return 0;  // No wifi :(

			case ConfigMem::WifiMac:
			case ConfigMem::WifiMac + 1:
			case ConfigMem::WifiMac + 2:
			case ConfigMem::WifiMac + 3:
			case ConfigMem::WifiMac + 4:
			case ConfigMem::WifiMac + 5: return MACAddress[vaddr - ConfigMem::WifiMac];

				default:
					notifyDataAbort(vaddr, false);
					LogUnmappedRead(8, vaddr);
					// Match Azahar/Citra behavior: data reads from unmapped regions return 0
					// instead of taking a synchronous data abort in the guest.
					return 0;
			}
		}
}

u16 Memory::read16(u32 vaddr) {
	const u32 page = vaddr >> pageShift;
	const u32 offset = vaddr & pageMask;

	uintptr_t pointer = activeVM().readTable[page];
	if (pointer != 0) [[likely]] {
		return *(u16*)(pointer + offset);
	} else {
		if (u8* privileged = resolvePrivilegedVirtualPointer(vaddr); privileged != nullptr) {
			u16 value {};
			std::memcpy(&value, privileged, sizeof(value));
			return value;
		}
		switch (vaddr) {
			case ConfigMem::WifiMac + 4: return (MACAddress[5] << 8) | MACAddress[4];  // Wifi MAC: Last 2 bytes of MAC Address
				default:
					notifyDataAbort(vaddr, false);
					LogUnmappedRead(16, vaddr);
					// Match Azahar/Citra behavior: data reads from unmapped regions return 0.
					return 0;
		}
	}
}

u32 Memory::read32(u32 vaddr) {
	const u32 page = vaddr >> pageShift;
	const u32 offset = vaddr & pageMask;

	uintptr_t pointer = activeVM().readTable[page];
	if (pointer != 0) [[likely]] {
		return *(u32*)(pointer + offset);
	} else {
		if (u8* privileged = resolvePrivilegedVirtualPointer(vaddr); privileged != nullptr) {
			u32 value {};
			std::memcpy(&value, privileged, sizeof(value));
			return value;
		}
		switch (vaddr) {
			case 0x1FF80000: return u32(kernelVersion) << 16;
			case ConfigMem::Datetime0: return u32(timeSince3DSEpoch());  // ms elapsed since Jan 1 1900, bottom 32 bits
			case ConfigMem::Datetime0 + 4:
				return u32(timeSince3DSEpoch() >> 32);  // top 32 bits
			// Ticks since time was last updated. For now we return the current tick count
			case ConfigMem::Datetime0 + 8: return u32(*cpuTicks);
			case ConfigMem::Datetime0 + 12: return u32(*cpuTicks >> 32);
			case ConfigMem::Datetime0 + 16: return 0xFFB0FF0;  // Unknown, set by PTM
			case ConfigMem::Datetime0 + 20:
			case ConfigMem::Datetime0 + 24:
			case ConfigMem::Datetime0 + 28: return 0;  // Set to 0 by PTM

			case ConfigMem::AppMemAlloc: return appResourceLimits.maxCommit;
			case ConfigMem::SyscoreVer: return 2;
			case 0x1FF81000:
				return 0;  // TODO: Figure out what this config mem address does
			// Wifi MAC: First 4 bytes of MAC Address
			case ConfigMem::WifiMac: return (u32(MACAddress[3]) << 24) | (u32(MACAddress[2]) << 16) | (u32(MACAddress[1]) << 8) | MACAddress[0];

			// 3D slider. Float in range 0.0 = off, 1.0 = max.
			case ConfigMem::SliderState3D: return Helpers::bit_cast<u32, float>(0.0f);
			case ConfigMem::FirmUnknown:
				return u32(read8(vaddr)) | (u32(read8(vaddr + 1)) << 8) | (u32(read8(vaddr + 2)) << 16) | (u32(read8(vaddr + 3)) << 24);

			default:
				if (vaddr >= VirtualAddrs::VramStart && vaddr < VirtualAddrs::VramStart + VirtualAddrs::VramSize) {
					static int shutUpCounter = 0;
					if (shutUpCounter < 5) {  // Stop spamming about VRAM reads after the first 5
						shutUpCounter++;
						Helpers::warn("VRAM read!\n");
					}

					// TODO: Properly handle framebuffer readbacks and the like
					return *(u32*)&vram[vaddr - VirtualAddrs::VramStart];
				}

						LogUnmappedRead(32, vaddr);
						notifyDataAbort(vaddr, false);
						// Match Azahar/Citra behavior: data reads from unmapped regions return 0.
						return 0;
		}
	}
}

u64 Memory::read64(u32 vaddr) {
	u64 bottom = u64(read32(vaddr));
	u64 top = u64(read32(vaddr + 4));
	return (top << 32) | bottom;
}

void Memory::write8(u32 vaddr, u8 value) {
	const u32 page = vaddr >> pageShift;
	const u32 offset = vaddr & pageMask;

	uintptr_t pointer = activeVM().writeTable[page];
	if (pointer != 0) [[likely]] {
		*(u8*)(pointer + offset) = value;
	} else {
		if (u8* privileged = resolvePrivilegedVirtualPointer(vaddr); privileged != nullptr) {
			*privileged = value;
			return;
		}
		// VRAM write
		if (vaddr >= VirtualAddrs::VramStart && vaddr < VirtualAddrs::VramStart + VirtualAddrs::VramSize) {
			// TODO: Invalidate renderer caches here
			vram[vaddr - VirtualAddrs::VramStart] = value;
		} else {
			notifyDataAbort(vaddr, true);
			LogUnmappedWrite(8, vaddr, value);
		}
	}
}

void Memory::write16(u32 vaddr, u16 value) {
	const u32 page = vaddr >> pageShift;
	const u32 offset = vaddr & pageMask;

	uintptr_t pointer = activeVM().writeTable[page];
	if (pointer != 0) [[likely]] {
		*(u16*)(pointer + offset) = value;
	} else {
		if (u8* privileged = resolvePrivilegedVirtualPointer(vaddr); privileged != nullptr) {
			std::memcpy(privileged, &value, sizeof(value));
			return;
		}
		notifyDataAbort(vaddr, true);
		LogUnmappedWrite(16, vaddr, value);
	}
}

void Memory::write32(u32 vaddr, u32 value) {
	const u32 page = vaddr >> pageShift;
	const u32 offset = vaddr & pageMask;

	uintptr_t pointer = activeVM().writeTable[page];
	if (pointer != 0) [[likely]] {
		*(u32*)(pointer + offset) = value;
	} else {
		if (u8* privileged = resolvePrivilegedVirtualPointer(vaddr); privileged != nullptr) {
			std::memcpy(privileged, &value, sizeof(value));
			return;
		}
		notifyDataAbort(vaddr, true);
		LogUnmappedWrite(32, vaddr, value);
	}
}

void Memory::write64(u32 vaddr, u64 value) {
	write32(vaddr, u32(value));
	write32(vaddr + 4, u32(value >> 32));
}

void* Memory::getReadPointer(u32 address) {
	const u32 page = address >> pageShift;
	const u32 offset = address & pageMask;

	uintptr_t pointer = activeVM().readTable[page];
	if (pointer == 0) {
		if (u8* privileged = resolvePrivilegedVirtualPointer(address); privileged != nullptr) {
			return privileged + offset;
		}
		return nullptr;
	}
	return (void*)(pointer + offset);
}

void* Memory::getWritePointer(u32 address) {
	const u32 page = address >> pageShift;
	const u32 offset = address & pageMask;

	uintptr_t pointer = activeVM().writeTable[page];
	if (pointer == 0) {
		if (u8* privileged = resolvePrivilegedVirtualPointer(address); privileged != nullptr) {
			return privileged + offset;
		}
		return nullptr;
	}
	return (void*)(pointer + offset);
}

// Thank you Citra devs
std::string Memory::readString(u32 address, u32 maxSize) {
	std::string string;
	string.reserve(maxSize);

	for (std::size_t i = 0; i < maxSize; ++i) {
		char c = read8(address++);
		if (c == '\0') {
			break;
		}

		string.push_back(c);
	}
	string.shrink_to_fit();

	return string;
}

// Return a pointer to the linear heap vaddr based on the kernel ver, because it needed to be moved
// thanks to the New 3DS having more FCRAM
u32 Memory::getLinearHeapVaddr() { return (kernelVersion < 0x22C) ? VirtualAddrs::LinearHeapStartOld : VirtualAddrs::LinearHeapStartNew; }

void Memory::changeMemoryState(u32 vaddr, s32 pages, const Operation& op) {
	assert(!(vaddr & 0xFFF));

	if (!op.changePerms && !op.changeState) Helpers::panic("Invalid op passed to changeMemoryState!");

	bool blockFound = false;

	for (auto it = memoryInfo.begin(); it != memoryInfo.end(); it++) {
		// Find the block that the memory region is located in
		u32 blockStart = it->baseAddr;
		u32 blockEnd = it->end();

		u32 reqStart = vaddr;
		u32 reqEnd = vaddr + (pages << 12);

		if (!(reqStart >= blockStart && reqEnd <= blockEnd)) continue;

		// Now that the block has been found, fill it with the necessary info
		auto oldState = it->state;
		u32 oldPerms = it->perms;
		it->baseAddr = reqStart;
		it->pages = pages;
		if (op.changePerms) it->perms = (op.r ? PERMISSION_R : 0) | (op.w ? PERMISSION_W : 0) | (op.x ? PERMISSION_X : 0);
		if (op.changeState) it->state = op.newState;

		// If the requested memory region is smaller than the block found, the block must be split
		if (blockStart < reqStart) {
			MemoryInfo startBlock(blockStart, (reqStart - blockStart) >> 12, oldPerms, oldState);
			memoryInfo.insert(it, startBlock);
		}

		if (reqEnd < blockEnd) {
			auto itAfter = std::next(it);
			MemoryInfo endBlock(reqEnd, (blockEnd - reqEnd) >> 12, oldPerms, oldState);
			memoryInfo.insert(itAfter, endBlock);
		}

		blockFound = true;
		break;
	}

	if (!blockFound) Helpers::panic("Unable to find block in changeMemoryState!");

	// Merge all blocks with the same state and permissions
	for (auto it = memoryInfo.begin(); it != memoryInfo.end();) {
		auto next = std::next(it);
		if (next == memoryInfo.end()) break;

		if (it->state != next->state || it->perms != next->perms) {
			it++;
			continue;
		}

		next->baseAddr = it->baseAddr;
		next->pages += it->pages;
		it = memoryInfo.erase(it);
	}
}

void Memory::queryPhysicalBlocks(FcramBlockList& outList, u32 vaddr, s32 pages) {
	s32 srcPages = pages;
	for (auto& alloc : memoryInfo) {
		u32 blockStart = alloc.baseAddr;
		u32 blockEnd = alloc.end();

		if (!(vaddr >= blockStart && vaddr < blockEnd)) continue;

		u32 blockPaddr = activeVM().paddrTable[vaddr >> 12];
		s32 blockPages = alloc.pages - ((vaddr - blockStart) >> 12);
		blockPages = std::min(srcPages, blockPages);
		FcramBlock physicalBlock(blockPaddr, blockPages);
		outList.push_back(physicalBlock);

		vaddr += blockPages << 12;
		srcPages -= blockPages;
		if (srcPages == 0) break;
	}

	if (srcPages != 0) Helpers::panic("Unable to find virtual pages to map!");
}

u32 Memory::normalizePhysicalAddress(u32 paddr) const {
	// FCRAM allocations are tracked as offsets, but memory mapping code works on the
	// physical map used by the 3DS.
	if (paddr < FCRAM_SIZE) {
		return PhysicalAddrs::FCRAM + paddr;
	}

	return paddr;
}

u8* Memory::resolvePhysicalPointer(u32 paddr) const {
	const u32 normalized = normalizePhysicalAddress(paddr);
	const auto region = ResolvePhysicalRegion(fcram, dspRam, vram, normalized);
	if (!region.has_value()) {
		return nullptr;
	}
	return region->hostPtr;
}

u8* Memory::resolvePrivilegedVirtualPointer(u32 vaddr) const {
	if ((vaddr & (1u << 31)) == 0) {
		return nullptr;
	}

	const u32 paddr = vaddr & ~(1u << 31);
	return resolvePhysicalPointer(paddr);
}

void Memory::notifyDataAbort(u32 vaddr, bool write) const {
	if (!mmuFaultCallback) {
		return;
	}

	(void)write;
	mmuFaultCallback(0x5, vaddr, false);
}

void Memory::mapPhysicalMemory(u32 vaddr, u32 paddr, s32 pages, bool r, bool w, bool x) {
	assert(!(vaddr & 0xFFF));
	assert(!(paddr & 0xFFF));

	paddr = normalizePhysicalAddress(paddr);
	const size_t mapSize = usize(pages) * pageSize;
	const auto region = ResolvePhysicalRegion(fcram, dspRam, vram, paddr);
	if (!region.has_value()) {
		Helpers::panic("Memory::mapPhysicalMemory: unknown physical address %08X", paddr);
	}
	if (region->size < mapSize) {
		Helpers::panic("Memory::mapPhysicalMemory: mapping overruns physical region %08X (+%zu)", paddr, mapSize);
	}

	if (useFastmem) {
		if (paddr >= PhysicalAddrs::FCRAM && paddr < PhysicalAddrs::FCRAM + FCRAM_SIZE) {
			addFastmemView(vaddr, FASTMEM_FCRAM_OFFSET + (paddr - PhysicalAddrs::FCRAM), mapSize, w);
		} else if (paddr >= PhysicalAddrs::DSP_RAM && paddr < PhysicalAddrs::DSP_RAM + DSP_RAM_SIZE) {
			addFastmemView(vaddr, FASTMEM_DSP_RAM_OFFSET + (paddr - PhysicalAddrs::DSP_RAM), mapSize, w);
		}
	}

	for (int i = 0; i < pages; i++) {
		u32 index = (vaddr >> 12) + i;
		activeVM().paddrTable[index] = paddr + (i << 12);
		activeVM().pageTableAttrs[index] = PageType::Memory;
		if (r)
			activeVM().readTable[index] = (uintptr_t)(region->hostPtr + (i << 12));
		else
			activeVM().readTable[index] = 0;

		if (w)
			activeVM().writeTable[index] = (uintptr_t)(region->hostPtr + (i << 12));
		else
			activeVM().writeTable[index] = 0;
	}
}

void Memory::unmapPhysicalMemory(u32 vaddr, u32 paddr, s32 pages) {
	for (int i = 0; i < pages; i++) {
		u32 index = (vaddr >> 12) + i;
		activeVM().paddrTable[index] = 0;
		activeVM().pageTableAttrs[index] = PageType::Unmapped;
		activeVM().readTable[index] = 0;
		activeVM().writeTable[index] = 0;
	}

	if (useFastmem) {
		arena->Unmap(vaddr, pages * pageSize, false);
	}
}

bool Memory::allocMemory(u32 vaddr, s32 pages, FcramRegion region, bool r, bool w, bool x, MemoryState state) {
	auto res = testMemoryState(vaddr, pages, MemoryState::Free);
	if (res.isFailure()) return false;

	FcramBlockList memList;
	fcramManager.alloc(memList, pages, region, false);

	for (auto it = memList.begin(); it != memList.end(); it++) {
		Operation op{.newState = state, .r = r, .w = w, .x = x, .changeState = true, .changePerms = true};
		changeMemoryState(vaddr, it->pages, op);
		mapPhysicalMemory(vaddr, it->paddr, it->pages, r, w, x);
		vaddr += it->pages << 12;
	}

	return true;
}

bool Memory::allocMemoryLinear(u32& outVaddr, u32 inVaddr, s32 pages, FcramRegion region, bool r, bool w, bool x) {
	if (inVaddr) Helpers::warn("inVaddr specified for linear allocation!");

	FcramBlockList memList;
	fcramManager.alloc(memList, pages, region, true);

	u32 paddr = memList.begin()->paddr;
	u32 vaddr = getLinearHeapVaddr() + paddr;
	auto res = testMemoryState(vaddr, pages, MemoryState::Free);
	if (res.isFailure()) Helpers::panic("Unable to map linear allocation (vaddr:%08X pages:%08X)", vaddr, pages);

	Operation op{.newState = MemoryState::Continuous, .r = r, .w = w, .x = x, .changeState = true, .changePerms = true};
	changeMemoryState(vaddr, pages, op);
	mapPhysicalMemory(vaddr, paddr, pages, r, w, x);

	outVaddr = vaddr;
	return true;
}

bool Memory::mapVirtualMemory(
	u32 dstVaddr, u32 srcVaddr, s32 pages, bool r, bool w, bool x, MemoryState oldDstState, MemoryState oldSrcState, MemoryState newDstState,
	MemoryState newSrcState, bool unmapPages
) {
	// Check that the regions have the specified state
	// TODO: check src perms
	auto res = testMemoryState(srcVaddr, pages, oldSrcState);
	if (res.isFailure()) return false;

	res = testMemoryState(dstVaddr, pages, oldDstState);
	if (res.isFailure()) return false;

	// Change the virtual memory state for both regions
	Operation srcOp{.newState = newSrcState, .changeState = true};
	changeMemoryState(srcVaddr, pages, srcOp);

	Operation dstOp{.newState = newDstState, .r = r, .w = w, .x = x, .changeState = true, .changePerms = true};
	changeMemoryState(dstVaddr, pages, dstOp);

	// Get a list of physical blocks in the source region
	FcramBlockList physicalList;
	queryPhysicalBlocks(physicalList, srcVaddr, pages);

	// Map or unmap each physical block
	for (auto& block : physicalList) {
		if (newDstState == MemoryState::Free) {
			// TODO: Games with CROs will unmap the CRO yet still continue accessing the address it was mapped to?
			if (unmapPages) {
				unmapPhysicalMemory(dstVaddr, block.paddr, block.pages);
			}
		} else {
			mapPhysicalMemory(dstVaddr, block.paddr, block.pages, r, w, x);
		}

		dstVaddr += block.pages << 12;
	}

	return true;
}

void Memory::changePermissions(u32 vaddr, s32 pages, bool r, bool w, bool x) {
	Operation op{.r = r, .w = w, .x = x, .changePerms = true};
	changeMemoryState(vaddr, pages, op);

	// Now that permissions have been changed, update the corresponding host tables
	FcramBlockList physicalList;
	queryPhysicalBlocks(physicalList, vaddr, pages);

	for (auto& block : physicalList) {
		mapPhysicalMemory(vaddr, block.paddr, block.pages, r, w, x);
		vaddr += block.pages;
	}
}

Result::HorizonResult Memory::queryMemory(MemoryInfo& out, u32 vaddr) {
	// Check each allocation
	for (auto& alloc : memoryInfo) {
		// Check if the memory address belongs in this allocation and return the info if so
		if (vaddr >= alloc.baseAddr && vaddr < alloc.end()) {
			out = alloc;
			return Result::Success;
		}
	}

	// Official kernel just returns an error here
	Helpers::warn("Failed to find block in QueryMemory!");
	return Result::FailurePlaceholder;
}

Result::HorizonResult Memory::testMemoryState(u32 vaddr, s32 pages, MemoryState desiredState) {
	for (auto& alloc : memoryInfo) {
		// Don't bother checking if we're to the left of the requested region
		if (vaddr >= alloc.end()) continue;
		if (alloc.state != desiredState) return Result::FailurePlaceholder;  // TODO: error for state mismatch

		// If the end of this block comes after the end of the requested range with no errors, it's a success
		if (alloc.end() >= vaddr + (pages << 12)) return Result::Success;
	}

	// TODO: error for when address is outside of userland
	return Result::FailurePlaceholder;
}

void Memory::copyToVaddr(u32 dstVaddr, const u8* srcHost, s32 size) {
	// TODO: check for noncontiguous allocations
	u8* dstHost = (u8*)activeVM().readTable[dstVaddr >> 12] + (dstVaddr & 0xFFF);
	memcpy(dstHost, srcHost, size);
}

u8* Memory::mapSharedMemory(Handle handle, u32 vaddr, u32 myPerms, u32 otherPerms) {
	for (auto& e : sharedMemBlocks) {
		if (e.handle == handle) {
			// Virtual Console titles trigger this. TODO: Investigate how it should work
			if (e.mapped) Helpers::warn("Allocated shared memory block twice. Is this allowed?");

			const u32 paddr = e.paddr;
			const u32 size = e.size;

			if (myPerms == 0x10000000) {
				myPerms = 3;
				Helpers::panic("Memory::mapSharedMemory with DONTCARE perms");
			}

			bool r = myPerms & 0b001;
			bool w = myPerms & 0b010;
			bool x = myPerms & 0b100;

			Operation op{.newState = MemoryState::Shared, .r = r, .w = x, .x = x, .changeState = true, .changePerms = true};
			changeMemoryState(vaddr, size >> 12, op);
			mapPhysicalMemory(vaddr, paddr, size >> 12, r, w, x);

			e.mapped = true;
			return &fcram[paddr];
		}
	}

	// This should be unreachable but better safe than sorry
	Helpers::panic("Memory::mapSharedMemory: Unknown shared memory handle %08X", handle);
	return nullptr;
}

u8* Memory::getSharedMemoryPointer(Handle handle) {
	for (auto& e : sharedMemBlocks) {
		if (e.handle == handle) {
			if (e.size == 0) {
				return nullptr;
			}
			return &fcram[e.paddr];
		}
	}
	return nullptr;
}

// Get the number of ms since Jan 1 1900
u64 Memory::timeSince3DSEpoch() {
	using namespace std::chrono;

	std::time_t rawTime = std::time(nullptr);   // Get current UTC time
	auto localTime = std::localtime(&rawTime);  // Convert to local time

	bool daylightSavings = localTime->tm_isdst > 0;  // Get if time includes DST
	localTime = std::gmtime(&rawTime);

	// Use gmtime + mktime to calculate difference between local time and UTC
	auto timezoneDifference = rawTime - std::mktime(localTime);
	if (daylightSavings) {
		timezoneDifference += 60ull * 60ull;  // Add 1 hour (60 seconds * 60 minutes)
	}

	// seconds between Jan 1 1900 and Jan 1 1970
	constexpr u64 offset = 2208988800ull;
	milliseconds ms = duration_cast<milliseconds>(seconds(rawTime + timezoneDifference + offset));
	return ms.count();
}

Regions Memory::getConsoleRegion() {
	// TODO: Let the user force the console region as they want
	// For now we pick one based on the ROM header
	return region;
}

void Memory::copySharedFont(u8* pointer, u32 vaddr) {
	const std::vector<u8> font = LoadSharedFontReplacement();
	if (font.empty()) {
		Helpers::warn("SharedFontReplacement.bin not found or empty\n");
		return;
	}
	std::memcpy(pointer, font.data(), font.size());

	// Relocate shared font to the address it's being loaded to
	HLE::Fonts::relocateSharedFont(pointer, vaddr);
}

std::optional<u64> Memory::getProgramID() {
	auto cxi = getCXI();

	if (cxi) {
		return cxi->programID;
	}

	return std::nullopt;
}
