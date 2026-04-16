#pragma once

#include "../renderer_software/renderer_software.h"

class RendererMetal final : public RendererSoftware {
public:
    RendererMetal();
    ~RendererMetal() override;

    void Init() override;
    void SwapBuffers() override;
    void ShutDown() override;

private:
    bool InitializeMetalResources();
    void PresentFrame() override;

    bool m_metal_available;
};
