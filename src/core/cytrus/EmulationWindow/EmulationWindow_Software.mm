#import "EmulationWindow_Software.h"

#import <UIKit/UIKit.h>

#ifdef __cplusplus
#include <algorithm>

#include "common/settings.h"
#endif

EmulationWindow_Software::EmulationWindow_Software(CA::MetalLayer* surface, bool is_secondary,
                                                   CGSize size)
    : Frontend::EmuWindow(is_secondary), host_window(surface), m_size(size) {
    if (Settings::values.layout_option.GetValue() == Settings::LayoutOption::SeparateWindows)
        is_portrait = false;
    else
        is_portrait = true;

    if (!surface)
        return;

    window_width = m_size.width;
    window_height = m_size.height;

    CreateWindowSurface();
    OnFramebufferSizeChanged();
}

EmulationWindow_Software::~EmulationWindow_Software() {
    DestroyWindowSurface();
    DestroyContext();
}

void EmulationWindow_Software::PollEvents() {}

void EmulationWindow_Software::OnSurfaceChanged(CA::MetalLayer* surface) {
    render_window = surface;

    window_info.type = Frontend::WindowSystemType::MacOS;
    window_info.render_surface = surface;
    window_info.render_surface_scale = [[UIScreen mainScreen] nativeScale];

    StopPresenting();
    OnFramebufferSizeChanged();
}

void EmulationWindow_Software::SizeChanged(CGSize size) {
    m_size = size;
    window_width = m_size.width;
    window_height = m_size.height;
}

void EmulationWindow_Software::OrientationChanged(UIInterfaceOrientation orientation,
                                                  CA::MetalLayer* surface) {
    if (Settings::values.layout_option.GetValue() != Settings::LayoutOption::SeparateWindows)
        is_portrait = UIInterfaceOrientationIsPortrait(orientation);
    OnSurfaceChanged(surface);
}

void EmulationWindow_Software::OnTouchEvent(int x, int y) {
    TouchPressed(static_cast<unsigned>(std::max(x, 0)), static_cast<unsigned>(std::max(y, 0)));
}

void EmulationWindow_Software::OnTouchMoved(int x, int y) {
    TouchMoved(static_cast<unsigned>(std::max(x, 0)), static_cast<unsigned>(std::max(y, 0)));
}

void EmulationWindow_Software::OnTouchReleased() {
    TouchReleased();
}

void EmulationWindow_Software::DoneCurrent() {}
void EmulationWindow_Software::MakeCurrent() {}

void EmulationWindow_Software::StopPresenting() {}
void EmulationWindow_Software::TryPresenting() {}

void EmulationWindow_Software::OnFramebufferSizeChanged() {
    auto bigger{window_width > window_height ? window_width : window_height};
    auto smaller{window_width < window_height ? window_width : window_height};

    UpdateCurrentFramebufferLayout(is_portrait ? smaller : bigger, is_portrait ? bigger : smaller,
                                   is_portrait);
}

bool EmulationWindow_Software::CreateWindowSurface() {
    if (!host_window)
        return true;

    window_info.render_surface = host_window;
    window_info.type = Frontend::WindowSystemType::MacOS;
    window_info.render_surface_scale = [[UIScreen mainScreen] nativeScale];

    return true;
}

void EmulationWindow_Software::DestroyContext() {}
void EmulationWindow_Software::DestroyWindowSurface() {}
