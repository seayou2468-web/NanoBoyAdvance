#ifndef OPENGLSUPPORT_H
#define OPENGLSUPPORT_H

#include "PlatformOGL.h"

namespace OpenGL {

bool BuildShaderProgram(const char*, const char*, GLuint*, const char*);
bool LinkShaderProgram(GLuint*);
void DeleteShaderProgram(GLuint*);
void UseShaderProgram(GLuint*);

}  // namespace OpenGL

#endif
