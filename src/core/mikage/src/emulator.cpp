#include "../include/emulator.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <fstream>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

namespace {

constexpr u32 kCompositeWidth = Emulator::width;
constexpr u32 kCompositeHeight = Emulator::height;
constexpr u32 kTopScreenTargetHeight = 240;

std::filesystem::path ResolveWritableBasePath() {
#if defined(__APPLE__) && TARGET_OS_IPHONE
	std::error_code ec;
	const std::filesystem::path cwd = std::filesystem::current_path(ec);
	if (!ec && cwd.is_absolute()) {
		if (cwd.filename() == "Documents") {
			return cwd;
		}
		const std::filesystem::path docs = cwd / "Documents";
		if (std::filesystem::exists(docs, ec) && !ec) {
			return docs;
		}
	}

	if (const char* home = std::getenv("HOME"); home != nullptr && home[0] != '\0') {
		std::filesystem::path home_path(home);
		if (home_path.is_absolute()) {
			if (home_path.filename() == "Documents") {
				return home_path;
			}
			return home_path / "Documents";
		}
	}
#endif

	if (const char* home = std::getenv("HOME"); home != nullptr && home[0] != '\0') {
		return std::filesystem::path(home);
	}

	return std::filesystem::current_path();
}

u32 ConvertToRGBA8888(const u8* pixel, PICA::ColorFmt format) {
	switch (format) {
		case PICA::ColorFmt::RGBA8:
			return (static_cast<u32>(pixel[0]) << 24) | (static_cast<u32>(pixel[1]) << 16) |
			       (static_cast<u32>(pixel[2]) << 8) | static_cast<u32>(pixel[3]);
		case PICA::ColorFmt::RGB8:
			return 0xFF000000u | (static_cast<u32>(pixel[0]) << 16) | (static_cast<u32>(pixel[1]) << 8) |
			       static_cast<u32>(pixel[2]);
		case PICA::ColorFmt::RGBA5551: {
			const u16 raw = static_cast<u16>(pixel[0] | (static_cast<u16>(pixel[1]) << 8));
			const u8 r = static_cast<u8>(((raw >> 11) & 0x1F) * 255 / 31);
			const u8 g = static_cast<u8>(((raw >> 6) & 0x1F) * 255 / 31);
			const u8 b = static_cast<u8>(((raw >> 1) & 0x1F) * 255 / 31);
			const u8 a = (raw & 0x1) ? 0xFF : 0x00;
			return (static_cast<u32>(a) << 24) | (static_cast<u32>(r) << 16) | (static_cast<u32>(g) << 8) |
			       static_cast<u32>(b);
		}
		case PICA::ColorFmt::RGB565: {
			const u16 raw = static_cast<u16>(pixel[0] | (static_cast<u16>(pixel[1]) << 8));
			const u8 r = static_cast<u8>(((raw >> 11) & 0x1F) * 255 / 31);
			const u8 g = static_cast<u8>(((raw >> 5) & 0x3F) * 255 / 63);
			const u8 b = static_cast<u8>((raw & 0x1F) * 255 / 31);
			return 0xFF000000u | (static_cast<u32>(r) << 16) | (static_cast<u32>(g) << 8) | static_cast<u32>(b);
		}
		case PICA::ColorFmt::RGBA4: {
			const u16 raw = static_cast<u16>(pixel[0] | (static_cast<u16>(pixel[1]) << 8));
			const u8 r = static_cast<u8>(((raw >> 12) & 0xF) * 17);
			const u8 g = static_cast<u8>(((raw >> 8) & 0xF) * 17);
			const u8 b = static_cast<u8>(((raw >> 4) & 0xF) * 17);
			const u8 a = static_cast<u8>((raw & 0xF) * 17);
			return (static_cast<u32>(a) << 24) | (static_cast<u32>(r) << 16) | (static_cast<u32>(g) << 8) |
			       static_cast<u32>(b);
		}
		default: return 0xFF000000u;
	}
}

void BlitRotatedScreenToComposite(std::span<u32> out_pixels, u32 composite_y_offset, const u8* src_base, u32 src_width,
                                  u32 src_height, u32 src_stride, PICA::ColorFmt format) {
	if (!src_base || src_width == 0 || src_height == 0) {
		return;
	}

	const u32 bpp = static_cast<u32>(PICA::sizePerPixel(format));
	if (bpp == 0 || src_stride < src_width * bpp) {
		return;
	}

	const u32 rotated_width = src_height;
	const u32 rotated_height = src_width;
	const u32 clamped_width = std::min<u32>(rotated_width, kCompositeWidth);
	const u32 clamped_height = std::min<u32>(rotated_height, kCompositeHeight - composite_y_offset);
	const u32 x_offset = (kCompositeWidth > clamped_width) ? (kCompositeWidth - clamped_width) / 2 : 0;

	for (u32 y = 0; y < clamped_height; ++y) {
		for (u32 x = 0; x < clamped_width; ++x) {
			const u32 src_x = y;
			const u32 src_y = src_height - 1 - x;
			const u8* src_pixel = src_base + src_y * src_stride + src_x * bpp;
			const u32 dst_index = (composite_y_offset + y) * kCompositeWidth + (x_offset + x);
			out_pixels[dst_index] = ConvertToRGBA8888(src_pixel, format);
		}
	}
}

}  // namespace

Emulator::Emulator()
		: config(getConfigPath()), scheduler(), memory(kernel.fcramManager, config), cpu(memory, *this, scheduler), gpu(memory, config),
		  kernel(cpu, memory, gpu, config), cheats(memory, kernel.getServiceManager().getHID()), audioDevice(config.audioDeviceConfig),
		  running(false)
{
	cpu.bindKernel(kernel);
	memory.setMMUFaultCallback([this](u32 fsr, u32 far, bool instruction_fault) {
		cpu.reportMMUFault(fsr, far, instruction_fault);
		kernel.reportMMUFault(fsr, far, instruction_fault, cpu.getLastAbortReturnAdjust());
	});

	DSPService& dspService = kernel.getServiceManager().getDSP();

	dsp = Audio::makeDSPCore(config, memory, scheduler, dspService);
	dspService.setDSPCore(dsp.get());

	audioDevice.init(dsp->getSamples());

	reloadSettings();
	reset(ReloadOption::NoReload);
}

Emulator::~Emulator() {
	config.save();
	audioDevice.close();

}

void Emulator::reset(ReloadOption reload) {
	cpu.reset();
	gpu.reset();
	memory.reset();
	dsp->reset();

	// Reset scheduler and add a VBlank event
	scheduler.reset();

	// Kernel must be reset last because it depends on CPU/Memory state
	kernel.reset();

	// Reloading r13 and r15 needs to happen after everything has been reset
	// Otherwise resetting the kernel or cpu might nuke them
	cpu.setReg(13, VirtualAddrs::StackTop);  // Set initial SP

	// We're resetting without reloading the ROM, so yeet cheats
	if (reload == ReloadOption::NoReload) {
		cheats.reset();
	}

	// If a ROM is active and we reset, with the reload option enabled then reload it.
	// This is necessary to set up stack, executable memory, .data/.rodata/.bss all over again
	if (reload == ReloadOption::Reload && romType != ROMType::None && romPath.has_value()) {
		bool success = loadROM(romPath.value());
		if (!success) {
			romType = ROMType::None;
			romPath = std::nullopt;

			Helpers::panic("Failed to reload ROM. This should pause the emulator in the future GUI");
		}
	}
}

std::filesystem::path Emulator::getConfigPath() {
	std::filesystem::path localPath = std::filesystem::current_path() / "config.toml";

	if (std::filesystem::exists(localPath)) {
		return localPath;
	} else {
		const auto appDataRoot = getAppDataRoot();
		std::error_code ec;
		std::filesystem::create_directories(appDataRoot, ec);
		if (ec) {
			Helpers::warn("Failed to create app data directory %s (error: %s)\n", appDataRoot.string().c_str(), ec.message().c_str());
		}
		return appDataRoot / "config.toml";
	}
}

void Emulator::step() {}

// Only resume if a ROM is properly loaded
void Emulator::resume() {
	running = (romType != ROMType::None);

	if (running && config.audioEnabled) {
		audioDevice.start();
	}
}

void Emulator::pause() {
	running = false;
	audioDevice.stop();
}

void Emulator::togglePause() { running ? pause() : resume(); }

void Emulator::runFrame() {
	if (running) {
		cpu.runFrame();  // Run 1 frame of instructions
		gpu.display();   // Display graphics

		// Run cheats if any are loaded
		if (cheats.haveCheats()) [[unlikely]] {
			cheats.run();
		}
	} else if (romType != ROMType::None) {
		// If the emulator is not running and a game is loaded, we still want to display the framebuffer otherwise we will get weird
		// double-buffering issues
		gpu.display();
	}
}

void Emulator::pollScheduler() {
	auto& events = scheduler.events;

	// Pop events until there's none pending anymore
	while (scheduler.currentTimestamp >= scheduler.nextTimestamp) {
		// Read event timestamp and type, pop it from the scheduler and handle it
		auto [time, eventType] = std::move(*events.begin());
		events.erase(events.begin());

		scheduler.updateNextTimestamp();

		switch (eventType) {
			case Scheduler::EventType::VBlank:
				[[likely]] {
					// Signal that we've reached the end of a frame
					frameDone = true;

					// Send VBlank interrupts
					ServiceManager& srv = kernel.getServiceManager();
					srv.sendGPUInterrupt(GPUInterrupt::VBlank0);
					srv.sendGPUInterrupt(GPUInterrupt::VBlank1);

					// Queue next VBlank event
					scheduler.addEvent(Scheduler::EventType::VBlank, time + CPU::ticksPerSec / 60);
					break;
				}

			case Scheduler::EventType::ThreadWakeup: kernel.pollThreadWakeups(); break;
			case Scheduler::EventType::UpdateTimers: kernel.pollTimers(); break;
			case Scheduler::EventType::RunDSP: {
				dsp->runAudioFrame(time);
				break;
			}

			case Scheduler::EventType::SignalY2R: kernel.getServiceManager().getY2R().signalConversionDone(); break;
			case Scheduler::EventType::UpdateIR: kernel.getServiceManager().getIRUser().updateCirclePadPro(); break;

			default: {
				Helpers::panic("Scheduler: Unimplemented event type received: %d\n", static_cast<int>(eventType));
				break;
			}
		}
	}
}

// Get path for saving files (AppData on Windows, /home/user/.local/share/ApplicationName on Linux, etc)
// Inside that path, we be use a game-specific folder as well. Eg if we were loading a ROM called PenguinDemo.3ds, the savedata would be in
// %APPDATA%/Alber/PenguinDemo/SaveData on Windows, and so on. We do this because games save data in their own filesystem on the cart.
// If the portable build setting is enabled, then those saves go in the executable directory instead
std::filesystem::path Emulator::getAppDataRoot() {
	const std::filesystem::path base = ResolveWritableBasePath();
	return base / "Emulator Files";
}

bool Emulator::loadROM(const std::filesystem::path& path) {
	// Reset the emulator if we've already loaded a ROM
	if (romType != ROMType::None) {
		reset(ReloadOption::NoReload);
	}

	// Reset whatever state needs to be reset before loading a new ROM
	memory.loadedCXI = std::nullopt;
	memory.loaded3DSX = std::nullopt;

	const std::filesystem::path appDataPath = getAppDataRoot();
	const std::filesystem::path dataPath = appDataPath / path.filename().stem();
	const std::filesystem::path aesKeysPath = appDataPath / "sysdata" / "aes_keys.txt";
	const std::filesystem::path seedDBPath = appDataPath / "sysdata" / "seeddb.bin";

	std::error_code ec;
	std::filesystem::create_directories(dataPath, ec);
	if (ec) {
		Helpers::warn("Failed to create game data directory %s (error: %s)\n", dataPath.string().c_str(), ec.message().c_str());
	}

	IOFile::setAppDataDir(dataPath);

	// Open the text file containing our AES keys if it exists. We use the std::filesystem::exists overload that takes an error code param to
	// avoid the call throwing exceptions
	if (std::filesystem::exists(aesKeysPath, ec) && !ec) {
		aesEngine.loadKeys(aesKeysPath);
	}

	if (std::filesystem::exists(seedDBPath, ec) && !ec) {
		aesEngine.setSeedPath(seedDBPath);
	}

	kernel.initializeFS();
	auto extension = path.extension();
	if (extension == ".toml" || path.filename() == "config.toml") {
		Helpers::warn("Refusing to open config file as ROM: %s\n", path.string().c_str());
		romPath = std::nullopt;
		romType = ROMType::None;
		return false;
	}
	bool success;  // Tracks if we loaded the ROM successfully

	if (extension == ".elf" || extension == ".axf")
		success = loadELF(path);
	else if (extension == ".3ds" || extension == ".cci")
		success = loadNCSD(path, ROMType::NCSD);
	else if (extension == ".cxi" || extension == ".app" || extension == ".ncch")
		success = loadNCSD(path, ROMType::CXI);
	else if (extension == ".3dsx")
		success = load3DSX(path);
	else {
		printf("Unknown file type\n");
		success = false;
	}

	if (success) {
		const u32 entrypoint = cpu.getReg(15);
		const u32 entrypointAddr = entrypoint & ~1u;  // Ignore Thumb bit
		const bool mapped = memory.getReadPointer(entrypointAddr) != nullptr;
		KernelMemoryTypes::MemoryInfo entryInfo;
		const bool queryOk = memory.queryMemory(entryInfo, entrypointAddr).isSuccess();
		const bool executablePerm = queryOk && ((entryInfo.perms & KernelMemoryTypes::MemoryState::PERMISSION_X) != 0);
		if (!mapped || entrypointAddr == 0) {
			Helpers::warn(
				"Invalid entrypoint detected after ROM load: entry=%08X (mapped=%d, query_ok=%d, exec_perm=%d)\n", entrypoint,
				int(mapped), int(queryOk), int(executablePerm)
			);
			success = false;
			romType = ROMType::None;
			romPath = std::nullopt;
		} else {
			// Some ROM types/boot flows may not expose executable permissions in a way we can
			// reliably validate at this layer yet. Keep this as diagnostics, not a hard failure.
			if (!queryOk || !executablePerm) {
				Helpers::warn(
					"Entrypoint permission check inconclusive: entry=%08X (query_ok=%d, exec_perm=%d). Continuing ROM boot.\n",
					entrypoint, int(queryOk), int(executablePerm)
				);
			}
			romPath = path;
		}
	} else {
		romPath = std::nullopt;
		romType = ROMType::None;
	}

	if (success) {
		// Update the main thread entrypoint and SP so that the thread debugger can display them.
		kernel.setMainThreadEntrypointAndSP(cpu.getReg(15), cpu.getReg(13));
		// Snapshot canonical VM only after entrypoint and mapping validation.
		memory.snapshotActiveVMAsCanonical();
	}

	resume();  // Start the emulator
	return success;
}

bool Emulator::copyCompositeFrameRGBA(std::span<u32> out_pixels) {
	if (out_pixels.size() < static_cast<size_t>(kCompositeWidth) * static_cast<size_t>(kCompositeHeight)) {
		return false;
	}

	std::fill(out_pixels.begin(), out_pixels.end(), 0xFF000000u);

	using namespace PICA::ExternalRegs;
	auto& regs = gpu.getExtRegisters();

	const bool top_select_b = (regs[Framebuffer0Select] & 0x1) != 0;
	const bool bottom_select_b = (regs[Framebuffer1Select] & 0x1) != 0;

	const u32 top_addr = top_select_b ? regs[Framebuffer0BFirstAddr] : regs[Framebuffer0AFirstAddr];
	const u32 bottom_addr = bottom_select_b ? regs[Framebuffer1BFirstAddr] : regs[Framebuffer1AFirstAddr];

	const u32 top_size = regs[Framebuffer0Size];
	const u32 bottom_size = regs[Framebuffer1Size];

	const u32 top_width = top_size & 0xFFFF;
	const u32 top_height = top_size >> 16;
	const u32 bottom_width = bottom_size & 0xFFFF;
	const u32 bottom_height = bottom_size >> 16;

	const auto top_format = static_cast<PICA::ColorFmt>(regs[Framebuffer0Config] & 0x7);
	const auto bottom_format = static_cast<PICA::ColorFmt>(regs[Framebuffer1Config] & 0x7);

	const u32 top_stride = regs[Framebuffer0Stride];
	const u32 bottom_stride = regs[Framebuffer1Stride];

	const u8* top_ptr = gpu.getPointerPhys<u8>(top_addr);
	const u8* bottom_ptr = gpu.getPointerPhys<u8>(bottom_addr);

	const u32 top_bpp = static_cast<u32>(PICA::sizePerPixel(top_format));
	const u32 bottom_bpp = static_cast<u32>(PICA::sizePerPixel(bottom_format));

	const u32 safe_top_width = top_width != 0 ? top_width : 240;
	const u32 safe_top_height = top_height != 0 ? top_height : 400;
	const u32 safe_bottom_width = bottom_width != 0 ? bottom_width : 240;
	const u32 safe_bottom_height = bottom_height != 0 ? bottom_height : 320;
	const u32 safe_top_stride = (top_stride != 0) ? top_stride : (safe_top_width * std::max<u32>(1, top_bpp));
	const u32 safe_bottom_stride = (bottom_stride != 0) ? bottom_stride : (safe_bottom_width * std::max<u32>(1, bottom_bpp));

	static u64 missing_fb_warn_counter = 0;
	auto logMissingScreen = [&](const char* screen_name, u32 addr, u32 width, u32 height, u32 stride, u32 format) {
		missing_fb_warn_counter++;
		if ((missing_fb_warn_counter % 120) == 1) {
			Helpers::warn(
				"Composite frame missing %s screen buffer (addr=%08X size=%ux%u stride=%u format=%u). Keeping other screen.\n",
				screen_name, addr, width, height, stride, format
			);
		}
	};

	if (top_ptr != nullptr) {
		BlitRotatedScreenToComposite(out_pixels, 0, top_ptr, safe_top_width, safe_top_height, safe_top_stride, top_format);
	} else {
		logMissingScreen("top", top_addr, safe_top_width, safe_top_height, safe_top_stride, static_cast<u32>(top_format));
	}

	if (bottom_ptr != nullptr) {
		BlitRotatedScreenToComposite(
			out_pixels, kTopScreenTargetHeight, bottom_ptr, safe_bottom_width, safe_bottom_height, safe_bottom_stride, bottom_format
		);
	} else {
		logMissingScreen(
			"bottom", bottom_addr, safe_bottom_width, safe_bottom_height, safe_bottom_stride, static_cast<u32>(bottom_format)
		);
	}

	return true;
}

bool Emulator::loadAmiibo(const std::filesystem::path& path) {
	NFCService& nfc = kernel.getServiceManager().getNFC();
	return nfc.loadAmiibo(path);
}

// Used for loading both CXI and NCSD files since they are both so similar and use the same interface
// (We promote CXI files to NCSD internally for ease)
bool Emulator::loadNCSD(const std::filesystem::path& path, ROMType type) {
	romType = type;
	std::optional<NCSD> opt = (type == ROMType::NCSD) ? memory.loadNCSD(aesEngine, path) : memory.loadCXI(aesEngine, path);

	if (!opt.has_value()) {
		return false;
	}

	loadedNCSD = opt.value();
	cpu.setReg(15, loadedNCSD.entrypoint);

	if (loadedNCSD.entrypoint & 1) {
		Helpers::panic("Misaligned NCSD entrypoint; should this start the CPU in Thumb mode?");
	}

	return true;
}

bool Emulator::load3DSX(const std::filesystem::path& path) {
	std::optional<u32> entrypoint = memory.load3DSX(path);
	romType = ROMType::HB_3DSX;

	if (!entrypoint.has_value()) {
		return false;
	}

	cpu.setReg(15, entrypoint.value());  // Set initial PC

	return true;
}

bool Emulator::loadELF(const std::filesystem::path& path) {
	// We can't open a new file with this ifstream if it's associated with a file
	if (loadedELF.is_open()) {
		loadedELF.close();
	}

	loadedELF.open(path, std::ios_base::binary);  // Open ROM in binary mode
	romType = ROMType::ELF;

	return loadELF(loadedELF);
}

bool Emulator::loadELF(std::ifstream& file) {
	// Rewind ifstream
	loadedELF.clear();
	loadedELF.seekg(0);

	std::optional<u32> entrypoint = memory.loadELF(loadedELF);
	if (!entrypoint.has_value()) {
		return false;
	}

	cpu.setReg(15, entrypoint.value());  // Set initial PC
	if (entrypoint.value() & 1) {
		Helpers::panic("Misaligned ELF entrypoint. TODO: Check if ELFs can boot in thumb mode");
	}

	return true;
}

std::span<u8> Emulator::getSMDH() {
	switch (romType) {
		case ROMType::NCSD:
		case ROMType::CXI: return memory.getCXI()->smdh;
		default: {
			return std::span<u8>();
		}
	}
}

static void dumpRomFSNode(const RomFS::RomFSNode& node, const char* romFSBase, const std::filesystem::path& path) {
	for (auto& file : node.files) {
		const auto p = path / file->name;
		std::ofstream outFile(p);

		outFile.write(romFSBase + file->dataOffset, file->dataSize);
	}

	for (auto& directory : node.directories) {
		const auto newPath = path / directory->name;

		// Create the directory for the new folder
		std::error_code ec;
		std::filesystem::create_directories(newPath, ec);

		if (!ec) {
			dumpRomFSNode(*directory, romFSBase, newPath);
		}
	}
}

RomFS::DumpingResult Emulator::dumpRomFS(const std::filesystem::path& path) {
	using namespace RomFS;

	if (romType != ROMType::NCSD && romType != ROMType::CXI && romType != ROMType::HB_3DSX) {
		return DumpingResult::InvalidFormat;
	}

	// Contents of RomFS as raw bytes
	std::vector<u8> romFS;
	u64 size;

	if (romType == ROMType::HB_3DSX) {
		auto hb3dsx = memory.get3DSX();
		if (!hb3dsx->hasRomFs()) {
			return DumpingResult::NoRomFS;
		}
		size = hb3dsx->romFSSize;

		romFS.resize(size);
		hb3dsx->readRomFSBytes(&romFS[0], 0, size);
	} else {
		auto cxi = memory.getCXI();
		if (!cxi->hasRomFS()) {
			return DumpingResult::NoRomFS;
		}

		const u64 offset = cxi->romFS.offset;
		size = cxi->romFS.size;

		romFS.resize(size);
		cxi->readFromFile(memory.CXIFile, cxi->partitionInfo, &romFS[0], offset - cxi->fileOffset, size);
	}

	std::unique_ptr<RomFSNode> node = parseRomFSTree((uintptr_t)&romFS[0], size);
	dumpRomFSNode(*node, (const char*)&romFS[0], path);

	return DumpingResult::Success;
}

void Emulator::setAudioEnabled(bool enable) {
	// Don't enable audio if we didn't manage to find an audio device and initialize it properly, otherwise audio sync will break,
	// because the emulator will expect the audio device to drain the sample buffer, but there's no audio device running...
	enable = enable && audioDevice.isInitialized();

	if (!enable) {
		audioDevice.stop();
	} else if (enable && romType != ROMType::None && running) {
		// Don't start the audio device yet if there's no ROM loaded or the emulator is paused
		// Resume and Pause will handle it
		audioDevice.start();
	}

	dsp->setAudioEnabled(enable);
}

void Emulator::reloadSettings() {
	setAudioEnabled(config.audioEnabled);

	gpu.getRenderer()->setHashTextures(config.hashTextures);
}
