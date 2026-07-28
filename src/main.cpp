#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include "renderer.h"
#include "window.h"

int main()
{
    if (glfwInit() == GLFW_FALSE)
    {
        std::cerr << "Failed to initialize GLFW\n";
        return EXIT_FAILURE;
    }
    //Instantiate our classes
    constexpr int height = 600;
    constexpr int width = 600;
    Engine::Window window(width, height);
    Engine::Renderer renderer;
    
    
    
    if (glewInit() != GLEW_OK) {exit(EXIT_FAILURE);}
    glfwSwapInterval(1); // Turn on vysync

    

    while (!window.shouldClose())
    {
        renderer.clear();
        renderer.draw();
        window.swapBuffers();
        window.pollEvents();
    }

    std::cout << "Cleaning up and exiting now" << std::endl;
    glfwTerminate();
    return EXIT_SUCCESS;
}