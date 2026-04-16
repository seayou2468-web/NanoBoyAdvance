#include "GPU3D_OpenGL.h"

namespace GPU3D {

GLRenderer::GLRenderer() : Renderer3D(false) {}

bool GLRenderer::Init() { return false; }
void GLRenderer::DeInit() {}
void GLRenderer::Reset() {}
void GLRenderer::SetRenderSettings(GPU::RenderSettings&) {}
void GLRenderer::RenderFrame() {}
u32* GLRenderer::GetLine(int) { return FallbackLine; }
void GLRenderer::SetupAccelFrame() {}
void GLRenderer::PrepareCaptureFrame() {}

}  // namespace GPU3D
