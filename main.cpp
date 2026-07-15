#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

int main() {

    // ============================================================
    // 1. CPU-SIDE DATA
    // ============================================================
    float vertices[] = {
        //triangle 1
        0.5f, 0.5f, 0.0f, //top right
        0.5f, -0.5f, 0.0f, //bottom right
        -0.5f, -0.5f, 0.0f, //bottom left
        -0.5f, 0.5f, 0.0f}; //top left

    unsigned int indices[] = {
        0, 1, 3,
        1, 2, 3
    };
    const char *vertexShaderSource = "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "void main()\n"
    "{\n"
    " gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
    "}\0";
    const char* fragmentShaderSource =
    "#version 330 core\n"
    "out vec4 FragColor;\n"
    "void main()\n"
    "{\n"
    "    FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);\n"
    "}\n";


    // ============================================================
    // 2. INITIALIZE GLFW
    // ============================================================
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // ============================================================
    // 3. CREATE WINDOW + OPENGL CONTEXT
    // ============================================================
    GLFWwindow* window = glfwCreateWindow(800, 800, "First Game Engine", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // ============================================================
    // 4. LOAD OPENGL FUNCTIONS
    // ============================================================
    gladLoadGL();
    glViewport(0,0,800,800);
    glClearColor(.07f,.13f,.17f,1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glfwSwapBuffers(window);

    // ============================================================
    // 5. CREATE VERTEX BUFFER
    // ============================================================
    unsigned int VAO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    unsigned int VBO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    unsigned int EBO;
    glGenBuffers(1, &EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // ============================================================
    // 6. CREATE + COMPILE SHADERS
    // ============================================================
    unsigned int vertexShader;
    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    unsigned int fragmentShader;
    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    // ============================================================
    // 7. LINK SHADERS INTO A SHADER PROGRAM
    // ============================================================
    unsigned int shaderProgram;
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glUseProgram(shaderProgram);

    // ============================================================
    // 8. TELL OPENGL HOW TO READ THE VBO
    // ============================================================
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // ============================================================
    // 9. RENDER LOOP
    // ============================================================
    while (!glfwWindowShouldClose(window))
    {
        // Clear the previous frame
        glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Use our shaders
        glUseProgram(shaderProgram);

        // Use our saved vertex configuration
        glBindVertexArray(VAO);

        // Draw 3 vertices as one triangle
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

        // Show the finished frame
        glfwSwapBuffers(window);

        // Process keyboard, mouse, window-close events, etc.
        glfwPollEvents();
    }

    // ============================================================
    // 10. CLEANUP
    // ============================================================
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}