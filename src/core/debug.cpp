#include "core/debug.h"
#include <iostream>

void GLClearError()
{
    while (glGetError() != GL_NO_ERROR)
    {

    }
}

bool GLLogCall(const char* expression, const char* file, int line) 
{
    bool ok = true;
    while (GLenum error = glGetError()) 
    {
        std::cerr << "[OpenGL Error] (0x" << std::hex << error << std::dec << ") "
        << expression << " — " << file << ":" << line << "\n";
        ok = false;
    }
    return ok;
}