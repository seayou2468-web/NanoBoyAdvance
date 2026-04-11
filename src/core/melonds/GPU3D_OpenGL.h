#pragma once

#include "GPU3D.h"

namespace GPU3D {

class GLRenderer : public Renderer3D {
public:
    GLRenderer();
    ~GLRenderer() override = default;

    bool Init() override;
    void DeInit() override;
    void Reset() override;
    void SetRenderSettings(GPU::RenderSettings& settings) override;
    void RenderFrame() override;
    u32* GetLine(int line) override;

    void SetupAccelFrame();
    void PrepareCaptureFrame();

private:
    u32 FallbackLine[256]{};
};

}  // namespace GPU3D
