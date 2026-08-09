#pragma once
#include <GL/glew.h>
#include <cassert>

void GLClearError();

bool GLLogCall(const char* expression, const char* file, int line);

#define GLCall(expr) \
    GLClearError(); \
    expr; \
    assert(GLLogCall(#expr, __FILE__, __LINE__))



