#include "core/window.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <iostream>
#include <string_view>

int main(int argc, char** argv)
{
    const bool smokeTest = argc == 2 && std::string_view(argv[1]) == "--smoke-test";
    if (argc > 1 && !smokeTest) {
        std::cerr << "Usage: VoxelEngine [--smoke-test]\n";
        return EXIT_FAILURE;
    }
    try {
        voxel::Window window(!smokeTest);
        int frames = 0;
        while (!window.shouldClose() && (!smokeTest || frames < 3)) {
            glfwPollEvents();
            if (glfwGetKey(window.handle(), GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(window.handle(), GLFW_TRUE);
            }
            glClearColor(0.48f, 0.70f, 0.90f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            window.present();
            ++frames;
        }
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "Voxel Engine: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
