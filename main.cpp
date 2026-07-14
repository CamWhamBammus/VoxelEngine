#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

int main() {
    
    //Initialize GLFW
    glfwInit();

    //GLFW Window Settings
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    //Instantiate the window
    GLFWwindow* window = glfwCreateWindow(800, 800, "First Game Engine", NULL, NULL);

    //Debugging
    if (window == NULL)
    {
        std::cout << "Failed to create window\n";
        glfwTerminate();
        return -1;
    }
    //Make the window the current context
    glfwMakeContextCurrent(window);

    //Load the opengl functions
    gladLoadGL();

    //Make the viewport the whole screen
    glViewport(0,0,800,800);

    //Change the background buffer to teal
    glClearColor(.07f,.13f,.17f,1.0f);

    //Get rid of the current screens buffer
    glClear(GL_COLOR_BUFFER_BIT);

    //Swap in the new teal buffer
    glfwSwapBuffers(window);

    //Keep the window open until the x is clicked
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
    }
    
    //Clean up
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}