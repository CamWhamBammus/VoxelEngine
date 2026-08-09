#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <iostream>



int main()
{
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize glfw\n";
        return -1;
    }
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Voxel Engine", nullptr, nullptr);
    if (window == nullptr)
    {
        std::cerr << "Failed to create glfw window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    glewExperimental = GL_TRUE; // Needed for full core-profile support in GLEW.
    if (glewInit() != GLEW_OK) 
    {
        std::cerr << "Failed to initialize GLEW\n";
        return-1;
    }

    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << "\n";
    std::cout << glGetString(GL_RENDERER) << "\n";
    std::cout << glGetString(GL_VENDOR) << "\n";


    while(!glfwWindowShouldClose(window))
    {
        float color = (sin(static_cast<float>(glfwGetTime())) + 1.0f) / 2.0f;
        
        glClearColor(color,1.0f - color,.15f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}