#include "GPU_OpenGL.h"

namespace GPU {

bool GLCompositor::Init() { return false; }
void GLCompositor::DeInit() {}
void GLCompositor::Reset() {}
void GLCompositor::SetRenderSettings(RenderSettings&) {}
void GLCompositor::Stop() {}
void GLCompositor::RenderFrame() {}
void GLCompositor::BindOutputTexture(int) {}

}  // namespace GPU
