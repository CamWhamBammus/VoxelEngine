#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "core/debug.h"

#include <iostream>

const char* vertexShaderSource = R"(
    #version 410 core
    layout(location = 0) in vec2 aPos;
    void main() 
    {
        gl_Position = vec4(aPos, 0.0, 1.0);
    }
    )";
    const char* fragmentShaderSource = R"(
    #version 410 core
    out vec4 FragColor;
    void main() 
    {
        float red = gl_FragCoord.x / 1280.0;
        float green = gl_FragCoord.y / 720.0;
        float blue = (gl_FragCoord.x + gl_FragCoord.y)/(720.0 + 1280.0);
        FragColor = vec4(red, green, blue, 1.0);
    }
    )";
GLuint compileShader(GLenum type, const char* source) 
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) 
    {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Shader compilation failed:\n" << infoLog << "\n";
    }
    
    return shader;
}

GLuint createProgram(const char* vertexSrc, const char* fragmentSrc) 
{
    GLuint vs = compileShader(GL_VERTEX_SHADER, vertexSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentSrc);
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) 
    {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "Program linking failed:\n" << infoLog << "\n";
    }
    // The shader objects are only needed during linking; the program keeps
    // its own internal compiled representation afterward. We free them
    // immediately rather than leaking two objects we'll never use again.
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}


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
    /*
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << "\n";
    std::cout << glGetString(GL_RENDERER) << "\n";
    std::cout << glGetString(GL_VENDOR) << "\n";
    */
    float vertices[] = 
    {
        -0.5f, 0.5f, //0: top-left
        -0.5f, -0.5f, //1: bottom-left
        0.5f, -0.5f, //2: bottom-right
        0.5f, 0.5f, //3: top-right
    };

    unsigned int indices[] = 
    {
        0, 1, 2, //first triangle
        2, 3, 0, //second triangle
    };
    GLuint vao, vbo, ebo;
    GLCall(glGenVertexArrays(1, &vao));
    GLCall(glBindVertexArray(vao));

    GLCall(glGenBuffers(1, &vbo));
    GLCall(glBindBuffer(GL_ARRAY_BUFFER, vbo));
    GLCall(glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW));
    GLCall(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0));
    GLCall(glEnableVertexAttribArray(0));

    GLCall(glGenBuffers(1, &ebo));
    GLCall(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo));
    GLCall(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW));



    GLCall(glBindVertexArray(0));
    GLuint shaderProgram = createProgram(vertexShaderSource, fragmentShaderSource);
    
    while (!glfwWindowShouldClose(window)) 
    {
        GLCall(glClearColor(0.1f, 0.1f, 0.15f, 1.0f));
        GLCall(glClear(GL_COLOR_BUFFER_BIT));
        GLCall(glUseProgram(shaderProgram));
        GLCall(glBindVertexArray(vao));
        GLCall(glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0));
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    GLCall(glDeleteBuffers(1, &ebo));
    GLCall(glDeleteVertexArrays(1, &vao));
    GLCall(glDeleteBuffers(1, &vbo));
    GLCall(glDeleteProgram(shaderProgram));
    glfwTerminate();
    return 0;
}