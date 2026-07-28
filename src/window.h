#pragma once
#include <GLFW/glfw3.h>

namespace Engine
{
    class Window
    {
        private:
            int _width;
            int _height;
            GLFWwindow* _windowHandle;
        public:
            void pollEvents() const;
            void swapBuffers() const;
            Window(int width, int height);
            bool shouldClose() const;
            GLFWwindow* windowHandleGetter() const;
            ~Window();
            

    };
}