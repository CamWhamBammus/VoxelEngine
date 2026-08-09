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
        switch(error)
        {
            case GL_INVALID_ENUM:
                std::cerr << "GL_INVALID_ENUM: An invalid OpenGL enum was passed to a function.\n";
                break;
            case GL_INVALID_VALUE:
                std::cerr << "GL_INVALID_VALUE: A numeric argument is outside the allowed range.\n";
                break;
            case GL_INVALID_OPERATION:
                std::cerr << "GL_INVALID_OPERATION: This operation is not allowed in the current OpenGL state.\n";
                break;
            case GL_INVALID_FRAMEBUFFER_OPERATION:
                std::cerr << "GL_INVALID_FRAMEBUFFER_OPERATION: The currently bound framebuffer is incomplete.\n";
                break;
            case GL_OUT_OF_MEMORY:
                std::cerr << "GL_OUT_OF_MEMORY: OpenGL could not allocate enough memory to complete the operation.\n";
                break;
            default:
                std::cerr << "UNKNOWN_OPENGL_ERROR: OpenGL reported an unrecognized error code.";
                break;
        }
        std::cerr
            << "Error code: 0x" << std::hex << error << std::dec << "\n"
            << "Expression: " << expression << "\n"
            << "File: " << file << "\n"
            << "Line: " << line << "\n\n";
        ok = false;
    }
    return ok;
}