#pragma once

namespace GPU {

struct RenderSettings;

class GLCompositor {
public:
    GLCompositor() = default;
    GLCompositor(const GLCompositor&) = delete;
    GLCompositor& operator=(const GLCompositor&) = delete;

    bool Init();
    void DeInit();
    void Reset();
    void SetRenderSettings(RenderSettings& settings);
    void Stop();
    void RenderFrame();
    void BindOutputTexture(int buf);
};

}  // namespace GPU
