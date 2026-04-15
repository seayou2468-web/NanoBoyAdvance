#pragma once

#include "video_core/renderer_software/renderer_software.h"

class RendererMetal final : public RendererSoftware {
public:
    RendererMetal();
    ~RendererMetal() override;

    void Init() override;
    void SwapBuffers() override;
    void ShutDown() override;

private:
    bool m_metal_available;
};

