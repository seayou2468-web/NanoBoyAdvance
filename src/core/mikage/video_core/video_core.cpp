// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "../common/logging/log.h"
#include "../common/settings.h"
#ifdef ENABLE_SOFTWARE_RENDERER
#include "renderer_software/renderer_software.h"
#endif
#include "video_core.h"

namespace VideoCore {

std::unique_ptr<RendererBase> CreateRenderer(Frontend::EmuWindow& emu_window,
                                             Frontend::EmuWindow* secondary_window,
                                             Pica::PicaCore& pica, Core::System& system) {
    (void)secondary_window;
    const Settings::GraphicsAPI graphics_api = Settings::values.graphics_api.GetValue();
    switch (graphics_api) {
#ifdef ENABLE_SOFTWARE_RENDERER
    case Settings::GraphicsAPI::Software:
        return std::make_unique<SwRenderer::RendererSoftware>(system, pica, emu_window);
#endif
    default:
        LOG_CRITICAL(Render,
                     "Unsupported graphics API {} for iOS mikage build; using software renderer",
                     graphics_api);
#ifdef ENABLE_SOFTWARE_RENDERER
        return std::make_unique<SwRenderer::RendererSoftware>(system, pica, emu_window);
#else
#error "Software renderer must be enabled for iOS mikage build."
#endif
    }
}

} // namespace VideoCore
