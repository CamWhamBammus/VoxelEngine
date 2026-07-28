#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

#include "engine.h"

namespace Engine
{
    void Renderer::clear() const
    {
        glClearColor(0.1f,0.2f,0.3f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    void Renderer::draw() const
    {
        
    }
    
    //----------------------------

    Window::Window(int width, int height) 
        : _width{width},
          _height{height},
          _windowHandle{nullptr}
    {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
        _windowHandle = glfwCreateWindow(width,height,"Game Engine 1", nullptr, nullptr); 
        if (!_windowHandle)
        {
            throw std::runtime_error("Failed to create GLFW window");
        }      
        glfwMakeContextCurrent(_windowHandle);
    }
    
    void Window::pollEvents() const
    {
        glfwPollEvents();
    }

    void Window::swapBuffers() const
    {
        glfwSwapBuffers(_windowHandle);
    }

    bool Window::shouldClose() const
    {
        return glfwWindowShouldClose(_windowHandle);
    }

    GLFWwindow* Window::windowHandleGetter() const
    {
        return _windowHandle;
    }

    Window::~Window()
    {
        if (_windowHandle != nullptr)
        {
            glfwDestroyWindow(_windowHandle);
        } 
    }
}