#include "OpenGLSupport.h"

namespace OpenGL {

bool BuildShaderProgram(const char*, const char*, GLuint*, const char*) { return false; }
bool LinkShaderProgram(GLuint*) { return false; }
void DeleteShaderProgram(GLuint*) {}
void UseShaderProgram(GLuint*) {}

}  // namespace OpenGL
