#include "../include/emulator.hpp"

#include <algorithm>
#include <array>
#include <cctype>
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

enum class ContainerProbeType {
	Unknown,
	NCSD,
	NCCH,
	ELF,
	_3DSX,
};

static ContainerProbeType ProbeContainerType(const std::filesystem::path& path) {
	std::ifstream file(path, std::ios::binary);
	if (!file) {
		return ContainerProbeType::Unknown;
	}

	std::array<u8, 0x220> header{};
	file.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()));
	const std::streamsize read = file.gcount();
	if (read < 4) {
		return ContainerProbeType::Unknown;
	}

	if (header[0] == 0x7F && header[1] == 'E' && header[2] == 'L' && header[3] == 'F') {
		return ContainerProbeType::ELF;
	}
	if (header[0] == '3' && header[1] == 'D' && header[2] == 'S' && header[3] == 'X') {
		return ContainerProbeType::_3DSX;
	}
	if (read >= 0x104 && header[0x100] == 'N' && header[0x101] == 'C' && header[0x102] == 'S' && header[0x103] == 'D') {
		return ContainerProbeType::NCSD;
	}
	if (read >= 0x104 && header[0x100] == 'N' && header[0x101] == 'C' && header[0x102] == 'C' && header[0x103] == 'H') {
		return ContainerProbeType::NCCH;
	}
	return ContainerProbeType::Unknown;
}

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
	auto packRGBA = [](u8 r, u8 g, u8 b, u8 a) -> u32 {
		// Keep the returned u32 in little-endian byte layout [R, G, B, A]
		// so frontends that upload as RGBA8 consume it without channel swapping.
		return (static_cast<u32>(a) << 24) | (static_cast<u32>(b) << 16) | (static_cast<u32>(g) << 8) | static_cast<u32>(r);
	};
	switch (format) {
		case PICA::ColorFmt::RGBA8:
			// PICA software path stores 32-bit colour as A,B,G,R byte order in memory.
			// Mirror RendererSw decode order to avoid channel/alpha mismatch when composing.
			return packRGBA(pixel[3], pixel[2], pixel[1], pixel[0]);
		case PICA::ColorFmt::RGB8:
			// PICA software path stores 24-bit colour as B,G,R.
			return packRGBA(pixel[2], pixel[1], pixel[0], 0xFF);
		case PICA::ColorFmt::RGBA5551: {
			const u16 raw = static_cast<u16>(pixel[0] | (static_cast<u16>(pixel[1]) << 8));
			const u8 r = static_cast<u8>(((raw >> 11) & 0x1F) * 255 / 31);
			const u8 g = static_cast<u8>(((raw >> 6) & 0x1F) * 255 / 31);
			const u8 b = static_cast<u8>(((raw >> 1) & 0x1F) * 255 / 31);
			const u8 a = (raw & 0x1) ? 0xFF : 0x00;
			return packRGBA(r, g, b, a);
		}
		case PICA::ColorFmt::RGB565: {
			const u16 raw = static_cast<u16>(pixel[0] | (static_cast<u16>(pixel[1]) << 8));
			const u8 r = static_cast<u8>(((raw >> 11) & 0x1F) * 255 / 31);
			const u8 g = static_cast<u8>(((raw >> 5) & 0x3F) * 255 / 63);
			const u8 b = static_cast<u8>((raw & 0x1F) * 255 / 31);
			return packRGBA(r, g, b, 0xFF);
		}
		case PICA::ColorFmt::RGBA4: {
			const u16 raw = static_cast<u16>(pixel[0] | (static_cast<u16>(pixel[1]) << 8));
			const u8 r = static_cast<u8>(((raw >> 12) & 0xF) * 17);
			const u8 g = static_cast<u8>(((raw >> 8) & 0xF) * 17);
			const u8 b = static_cast<u8>(((raw >> 4) & 0xF) * 17);
			const u8 a = static_cast<u8>((raw & 0xF) * 17);
			return packRGBA(r, g, b, a);
		}
		default: return 0xFF000000u;
	}
}

bool BlitRotatedScreenToComposite(std::span<u32> out_pixels, u32 composite_y_offset, const u8* src_base, u32 src_width,
                                  u32 src_height, u32 src_stride, PICA::ColorFmt format) {
	if (!src_base || src_width == 0 || src_height == 0) {
		return false;
	}

	const u32 bpp = static_cast<u32>(PICA::sizePerPixel(format));
	if (bpp == 0 || src_stride < src_width * bpp) {
		Helpers::warn(
			"Composite blit rejected due to invalid geometry (w=%u h=%u stride=%u bpp=%u format=%u)",
			src_width, src_height, src_stride, bpp, static_cast<u32>(format)
		);
		return false;
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
	return true;
}

}  // namespace

Emulator::Emulator()
		: config(getConfigPath()),
		  scheduler(),
		  memory(kernel.fcramManager, config),
		  cpu(memory, scheduler, [this]() { pollScheduler(); }, [this]() { return frameDone; }, [this](bool done) { frameDone = done; }),
		  gpu(memory, config),
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
	IOFile::setUserDataDir(appDataPath);

	const std::filesystem::path dataPath = IOFile::getNANDData() / "title" / path.filename().stem();
	const std::filesystem::path aesKeysPath = IOFile::getSysData() / "aes_keys.txt";
	const std::filesystem::path seedDBPath = IOFile::getSysData() / "seeddb.bin";

	std::error_code ec;
	std::filesystem::create_directories(IOFile::getNANDData(), ec);
	std::filesystem::create_directories(IOFile::getSDMC(), ec);
	std::filesystem::create_directories(IOFile::getSharedFiles(), ec);
	std::filesystem::create_directories(IOFile::getSysData(), ec);

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
	std::string extension = path.extension().string();
	std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	const std::string filename = path.filename().string();
	const std::string filenameLower = [&] {
		std::string s = filename;
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
			return static_cast<char>(std::tolower(c));
		});
		return s;
	}();

	if (extension == ".toml" || filenameLower == "config.toml") {
		Helpers::warn("Refusing to open config file as ROM: %s\n", path.string().c_str());
		romPath = std::nullopt;
		romType = ROMType::None;
		return false;
	}
	bool success;  // Tracks if we loaded the ROM successfully

	ContainerProbeType probeType = ContainerProbeType::Unknown;
	// Prefer extension dispatch when known, and use probe as fallback/validation.
	if (extension == ".elf" || extension == ".axf") {
		success = loadELF(path);
	} else if (extension == ".3ds" || extension == ".cci") {
		success = loadNCSD(path, ROMType::NCSD);
	} else if (extension == ".cxi" || extension == ".app" || extension == ".ncch") {
		success = loadNCSD(path, ROMType::CXI);
	} else if (extension == ".3dsx") {
		success = load3DSX(path);
	} else {
		probeType = ProbeContainerType(path);
		Helpers::warn("ROM extension dispatch fallback: extension=\"%s\" probe=%d path=%s\n",
			extension.c_str(), int(probeType), path.string().c_str());
		switch (probeType) {
			case ContainerProbeType::ELF: success = loadELF(path); break;
			case ContainerProbeType::_3DSX: success = load3DSX(path); break;
			case ContainerProbeType::NCSD: success = loadNCSD(path, ROMType::NCSD); break;
			case ContainerProbeType::NCCH: success = loadNCSD(path, ROMType::CXI); break;
			default:
				printf("Unknown file type and unsupported container magic\n");
				success = false;
				break;
		}
	}

	if (success) {
		const u32 entrypoint = cpu.getReg(15);
		const u32 entrypointAddr = entrypoint & ~1u;  // Ignore Thumb bit
		const bool mapped = memory.getReadPointer(entrypointAddr) != nullptr;
		KernelMemoryTypes::MemoryInfo entryInfo;
		const bool queryOk = memory.queryMemory(entryInfo, entrypointAddr).isSuccess();
		const bool executablePerm = queryOk && ((entryInfo.perms & KernelMemoryTypes::MemoryState::PERMISSION_X) != 0);
		Helpers::warn(
			"ROM entrypoint probe: rom_type=%d entry=%08X canonical=%08X mapped=%d query_ok=%d exec_perm=%d\n",
			int(romType), entrypoint, entrypointAddr, int(mapped), int(queryOk), int(executablePerm)
		);
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
		std::filesystem::create_directories(dataPath, ec);
		if (ec) {
			Helpers::warn("Failed to create game data directory %s (error: %s)\n", dataPath.string().c_str(), ec.message().c_str());
		}

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

	const u32 top_primary_addr = top_select_b ? regs[Framebuffer0BFirstAddr] : regs[Framebuffer0AFirstAddr];
	const u32 top_primary_second_addr = top_select_b ? regs[Framebuffer0BSecondAddr] : regs[Framebuffer0ASecondAddr];
	const u32 top_fallback_addr = top_select_b ? regs[Framebuffer0AFirstAddr] : regs[Framebuffer0BFirstAddr];
	const u32 top_fallback_second_addr = top_select_b ? regs[Framebuffer0ASecondAddr] : regs[Framebuffer0BSecondAddr];

	const u32 bottom_primary_addr = bottom_select_b ? regs[Framebuffer1BFirstAddr] : regs[Framebuffer1AFirstAddr];
	const u32 bottom_primary_second_addr = bottom_select_b ? regs[Framebuffer1BSecondAddr] : regs[Framebuffer1ASecondAddr];
	const u32 bottom_fallback_addr = bottom_select_b ? regs[Framebuffer1AFirstAddr] : regs[Framebuffer1BFirstAddr];
	const u32 bottom_fallback_second_addr = bottom_select_b ? regs[Framebuffer1ASecondAddr] : regs[Framebuffer1BSecondAddr];

	const u32 top_size = regs[Framebuffer0Size];
	const u32 bottom_size = regs[Framebuffer1Size];

	const u32 top_width = top_size & 0xFFFF;
	const u32 top_height = top_size >> 16;
	const u32 bottom_width = bottom_size & 0xFFFF;
	const u32 bottom_height = bottom_size >> 16;

	const auto top_format = static_cast<PICA::ColorFmt>(regs[Framebuffer0Config] & 0x7);
	const auto bottom_format = static_cast<PICA::ColorFmt>(regs[Framebuffer1Config] & 0x7);
	const u32 top_bpp = static_cast<u32>(PICA::sizePerPixel(top_format));
	const u32 bottom_bpp = static_cast<u32>(PICA::sizePerPixel(bottom_format));

	const u32 top_stride = regs[Framebuffer0Stride];
	const u32 bottom_stride = regs[Framebuffer1Stride];
	auto& internalRegs = gpu.getRegisters();

	const u8* top_ptr = gpu.getPointerPhys<u8>(top_primary_addr);
	const u8* bottom_ptr = gpu.getPointerPhys<u8>(bottom_primary_addr);
	u32 top_addr = top_primary_addr;
	u32 bottom_addr = bottom_primary_addr;

	// Some titles/frontends can transiently expose an empty selected FB while another eye/buffer is valid.
	// Fall back across first/second and A/B buffers to avoid long black-screen streaks.
	if (top_ptr == nullptr) {
		const std::array<u32, 3> top_candidates = {
			top_primary_second_addr,
			top_fallback_addr,
			top_fallback_second_addr,
		};
		for (u32 candidate_addr : top_candidates) {
			const u8* alt = gpu.getPointerPhys<u8>(candidate_addr);
			if (alt != nullptr) {
				top_ptr = alt;
				top_addr = candidate_addr;
				break;
			}
		}
	}
	if (bottom_ptr == nullptr) {
		const std::array<u32, 3> bottom_candidates = {
			bottom_primary_second_addr,
			bottom_fallback_addr,
			bottom_fallback_second_addr,
		};
		for (u32 candidate_addr : bottom_candidates) {
			const u8* alt = gpu.getPointerPhys<u8>(candidate_addr);
			if (alt != nullptr) {
				bottom_ptr = alt;
				bottom_addr = candidate_addr;
				break;
			}
		}
	}

	auto looksLikeBlackLinearBuffer = [](const u8* ptr, u32 bpp, u32 stride, u32 width, u32 height) -> bool {
		if (ptr == nullptr || bpp == 0 || stride < width * bpp || width == 0 || height == 0) {
			return true;
		}
		const u32 sampleRows = std::min<u32>(height, 8);
		const u32 sampleCols = std::min<u32>(width, 8);
		for (u32 y = 0; y < sampleRows; ++y) {
			for (u32 x = 0; x < sampleCols * bpp; ++x) {
				if (ptr[y * stride + x] != 0) {
					return false;
				}
			}
		}
		return true;
	};

	// If GSP framebuffer routing is not initialized yet, or if the selected LCD buffer
	// remains all-zero while the PICA color buffer has content, fall back to the active
	// render color buffer. This keeps output alive for titles that delay SetBufferSwap.
	if (top_ptr == nullptr || looksLikeBlackLinearBuffer(top_ptr, top_bpp, top_stride, top_width != 0 ? top_width : 240, top_height != 0 ? top_height : 400)) {
		const u32 renderColorBuffer = PhysicalAddrs::VRAM + internalRegs[PICA::InternalRegs::ColourBufferLoc];
		const u8* renderPtr = gpu.getPointerPhys<u8>(renderColorBuffer);
		if (renderPtr != nullptr && !looksLikeBlackLinearBuffer(renderPtr, top_bpp, top_stride != 0 ? top_stride : (240 * std::max<u32>(1, top_bpp)), 240, 400)) {
			top_ptr = renderPtr;
			top_addr = renderColorBuffer;
		}
	}

	u32 safe_top_width = top_width != 0 ? top_width : 240;
	u32 safe_top_height = top_height != 0 ? top_height : 400;
	const u32 safe_bottom_width = bottom_width != 0 ? bottom_width : 240;
	const u32 safe_bottom_height = bottom_height != 0 ? bottom_height : 320;
	if (top_width == 0 && top_height == 0) {
		const u32 fbSizeReg = internalRegs[PICA::InternalRegs::FramebufferSize];
		const u32 picaWidth = fbSizeReg & 0x7FF;
		const u32 picaHeight = ((fbSizeReg >> 12) & 0x3FF) + 1;
		if (picaWidth != 0 && picaHeight != 0) {
			safe_top_width = picaWidth;
			safe_top_height = picaHeight;
		}
	}

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

	bool drewAnyScreen = false;

	if (top_ptr != nullptr) {
		if (BlitRotatedScreenToComposite(out_pixels, 0, top_ptr, safe_top_width, safe_top_height, safe_top_stride, top_format)) {
			drewAnyScreen = true;
		} else {
			logMissingScreen("top", top_addr, safe_top_width, safe_top_height, safe_top_stride, static_cast<u32>(top_format));
		}
	} else {
		logMissingScreen("top", top_addr, safe_top_width, safe_top_height, safe_top_stride, static_cast<u32>(top_format));
	}

	if (bottom_ptr != nullptr) {
		if (BlitRotatedScreenToComposite(
				out_pixels, kTopScreenTargetHeight, bottom_ptr, safe_bottom_width, safe_bottom_height, safe_bottom_stride, bottom_format
			)) {
			drewAnyScreen = true;
		} else {
			logMissingScreen(
				"bottom", bottom_addr, safe_bottom_width, safe_bottom_height, safe_bottom_stride, static_cast<u32>(bottom_format)
			);
		}
	} else {
		logMissingScreen(
			"bottom", bottom_addr, safe_bottom_width, safe_bottom_height, safe_bottom_stride, static_cast<u32>(bottom_format)
		);
	}

	if (drewAnyScreen) {
		bool any_visible_pixel = false;
		for (u32 px : out_pixels) {
			if (px != 0xFF000000u) {
				any_visible_pixel = true;
				break;
			}
		}
		if (!any_visible_pixel) {
			static u64 black_frame_warn_counter = 0;
			black_frame_warn_counter++;
			if ((black_frame_warn_counter % 120) == 1) {
				const u8 top_b0 = (top_ptr != nullptr) ? top_ptr[0] : 0;
				const u8 top_b1 = (top_ptr != nullptr) ? top_ptr[1] : 0;
				const u8 top_b2 = (top_ptr != nullptr) ? top_ptr[2] : 0;
				const u8 bottom_b0 = (bottom_ptr != nullptr) ? bottom_ptr[0] : 0;
				const u8 bottom_b1 = (bottom_ptr != nullptr) ? bottom_ptr[1] : 0;
				const u8 bottom_b2 = (bottom_ptr != nullptr) ? bottom_ptr[2] : 0;
				const u32 colorBufferAddr = PhysicalAddrs::VRAM + internalRegs[PICA::InternalRegs::ColourBufferLoc];
				const u8* colorBufferPtr = gpu.getPointerPhys<u8>(colorBufferAddr);
				const u8 color_b0 = (colorBufferPtr != nullptr) ? colorBufferPtr[0] : 0;
				const u8 color_b1 = (colorBufferPtr != nullptr) ? colorBufferPtr[1] : 0;
				const u8 color_b2 = (colorBufferPtr != nullptr) ? colorBufferPtr[2] : 0;
				Helpers::warn(
					"Composite output stayed black despite valid blit sources. top(addr=%08X fmt=%u stride=%u size=%ux%u head=%02X %02X %02X) "
					"bottom(addr=%08X fmt=%u stride=%u size=%ux%u head=%02X %02X %02X) "
					"color(addr=%08X head=%02X %02X %02X)",
					top_addr, static_cast<u32>(top_format), safe_top_stride, safe_top_width, safe_top_height,
					top_b0, top_b1, top_b2,
					bottom_addr, static_cast<u32>(bottom_format), safe_bottom_stride, safe_bottom_width, safe_bottom_height,
					bottom_b0, bottom_b1, bottom_b2,
					colorBufferAddr, color_b0, color_b1, color_b2
				);
			}
		}
	}

	return drewAnyScreen;
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
