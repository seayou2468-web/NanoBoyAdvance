#pragma once

#import <UIKit/UIKit.h>

#include "core/frontend/emu_window.h"

class EmulationWindow_Software : public Frontend::EmuWindow {
public:
    EmulationWindow_Software(CA::MetalLayer* surface, bool is_secondary, CGSize size);
    ~EmulationWindow_Software();

    void PollEvents() override;

    void OnSurfaceChanged(CA::MetalLayer* surface);

    void OnTouchEvent(int x, int y);
    void OnTouchMoved(int x, int y);
    void OnTouchReleased();

    void DoneCurrent() override;
    void MakeCurrent() override;

    void StopPresenting();
    void TryPresenting();

    int window_width;
    int window_height;

    void SizeChanged(CGSize size);
    void OrientationChanged(UIInterfaceOrientation orientation, CA::MetalLayer* surface);

protected:
    void OnFramebufferSizeChanged();

    bool CreateWindowSurface();
    void DestroyContext();
    void DestroyWindowSurface();

private:
    CA::MetalLayer* render_window;
    CA::MetalLayer* host_window;

    bool is_portrait;

    CGSize m_size;
};
