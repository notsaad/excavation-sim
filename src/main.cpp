#include <glad/gl.h>

#include <GLFW/glfw3.h>

#include <cstdlib>
#include <iostream>

static void errorCallback(int error, const char *description) {
  std::cerr << "GLFW error " << error << ": " << description << "\n";
}

static void keyCallback(GLFWwindow *window, int key, int /*scancode*/,
                        int action, int /*mods*/) {
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    glfwSetWindowShouldClose(window, GLFW_TRUE);
}

static void framebufferSizeCallback(GLFWwindow * /*window*/, int width,
                                    int height) {
  glViewport(0, 0, width, height);
}

int main() {
  glfwSetErrorCallback(errorCallback);

  if (!glfwInit()) {
    std::cerr << "Failed to initialize GLFW\n";
    return EXIT_FAILURE;
  }

  // Request OpenGL 3.3 core profile
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

  GLFWwindow *window =
      glfwCreateWindow(1280, 720, "Excavation Simulator", nullptr, nullptr);
  if (!window) {
    std::cerr << "Failed to create GLFW window\n";
    glfwTerminate();
    return EXIT_FAILURE;
  }

  glfwMakeContextCurrent(window);
  glfwSwapInterval(1); // vsync
  glfwSetKeyCallback(window, keyCallback);
  glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

  // Load OpenGL functions
  int version = gladLoadGL(glfwGetProcAddress);
  if (version == 0) {
    std::cerr << "Failed to initialize OpenGL context\n";
    glfwTerminate();
    return EXIT_FAILURE;
  }

  std::cout << "OpenGL " << GLAD_VERSION_MAJOR(version) << "."
            << GLAD_VERSION_MINOR(version) << "\n";
  std::cout << "Renderer: " << glGetString(GL_RENDERER) << "\n";

  glEnable(GL_DEPTH_TEST);
  glClearColor(0.53f, 0.81f, 0.92f, 1.0f);

  // array of three 3d points (x, y, z) to render a simple triangle
  float vertices[] = {-0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 0.0f, 0.0f, 0.5f, 0.0f};

  GLuint VBO; // VBO -> vertex buffer object
  GLuint VAO; // VAO -> vertex array object (how to read the vbo)

  glGenVertexArrays(1, &VAO);
  glBindVertexArray(VAO);

  glGenBuffers(1, &VBO); // just memory chunks fed from cpu to gpu to handle
                         // graphics rendering
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  // static draw tells the GPU data is set once and used multiple times
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices,
               GL_STATIC_DRAW); // copies user defined data into bound buffer

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0,
                        nullptr); // to describe the vertex layout for vertices
  glEnableVertexAttribArray(0);

  // shaders are written as raw c strings which is kind of weird

  // places the dots
  const char *vertexShaderSource = R"(
      #version 330 core
      layout (location = 0) in vec3 aPos;

      void main() {
        gl_Position = vec4(aPos, 1.0);
      }
  )";

  // colors the dots
  const char *fragmentShaderSource = R"(
      #version 330 core
      out vec4 FragColor;

      void main() {
        FragColor = vec4(1.0, 0.5, 0.2, 1.0);
      }
  )";

  // now need to render each individual shader
  GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
  GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

  // link the shaders to their source code
  glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
  glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);

  // compile shaders
  glCompileShader(vertexShader);
  glCompileShader(fragmentShader);

  int vertexSuccess;
  int fragmentSuccess;

  glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &vertexSuccess);
  glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &fragmentSuccess);
  if (!vertexSuccess || !fragmentSuccess)
    std::cout << "error occurred linking\n";

  // make a program for the gpu to execute
  GLuint program = glCreateProgram();

  glAttachShader(program, vertexShader);
  glAttachShader(program, fragmentShader);

  glLinkProgram(program);

  // can delete the shaders now since they're in the program
  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

  // this is the main render loop that runs 60 times per second (60FPS)
  while (!glfwWindowShouldClose(window)) {
    glClear(GL_COLOR_BUFFER_BIT |
            GL_DEPTH_BUFFER_BIT); // sets the background to chose colour

    // rendering the triangle
    glUseProgram(program);
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    // TODO: simulation step
    // TODO: render terrain
    // TODO: render bucket

    glfwSwapBuffers(window); // shows new frame
    glfwPollEvents();        // polls for actions
  }

  glfwDestroyWindow(window);
  glfwTerminate();
  return EXIT_SUCCESS;
}
