#pragma once
#include <GL/glew.h>
#include <cassert>

void CLClearError();

bool GLLogCall(const char* expression, const char* file, int line);

#define GLCall(expr) \
    GLClearError(); \
    expr; \
    assert(GLLogCall(#expr, _FILE_, _LINE_))



