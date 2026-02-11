#include <glad/gl.h>

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>                  // mat4, vec3, vec4 (core types)
#include <glm/gtc/matrix_transform.hpp> // lookAt, perspective (transformations)
#include <glm/gtc/type_ptr.hpp>         // value_ptr (convert to OpenGL pointer)

#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/trigonometric.hpp"
#include "rendering/shader.h"
#include <cstdlib>
#include <iostream>

// anon namespace instead of static functions
namespace {
// just called on errors
void errorCallback(int error, const char *description) {
  std::cerr << "GLFW error " << error << ": " << description << "\n";
}

// called on keyboard presses
void keyCallback(GLFWwindow *window, int key, int /*scancode*/, int action, int /*mods*/) {
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    glfwSetWindowShouldClose(window, GLFW_TRUE);
}

// called on window resizes
void framebufferSizeCallback(GLFWwindow * /*window*/, int width, int height) {
  glViewport(0, 0, width, height);
}
} // namespace

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

  GLFWwindow *window = glfwCreateWindow(1280, 720, "Excavation Simulator", nullptr, nullptr);
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

  std::cout << "OpenGL " << GLAD_VERSION_MAJOR(version) << "." << GLAD_VERSION_MINOR(version)
            << '\n';
  std::cout << "Renderer: " << glGetString(GL_RENDERER) << '\n';

  glEnable(GL_DEPTH_TEST);

  // set background colour
  glClearColor(0.53f, 0.81f, 0.92f, 1.0f);

  // array of three 3d points (x, y, z) to render a simple triangle
  float vertices[] = {-0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 0.0f, 0.0f, 0.5f, 0.0f};

  GLuint VBO; // VBO -> vertex buffer object
  GLuint VAO; // VAO -> vertex array object (how to read the vbo)

  glGenVertexArrays(1, &VAO);
  glBindVertexArray(VAO);

  // memory chunks fed from cpu to gpu to handle graphics rendering
  glGenBuffers(1, &VBO);

  // static draw tells the GPU data is set once and used multiple times
  glBindBuffer(GL_ARRAY_BUFFER, VBO);

  // copies user defined data into bound buffer
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  // to describe the vertex layout for vertices
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
  glEnableVertexAttribArray(0);

  // using shader class and giving path to shader code
  Shader basic_shader("shaders/basic.vert", "shaders/basic.frag");
  
  // model matrix (4x4 identity matrix)
  glm::mat4 model = glm::mat4(1.0f);
  
  glm::vec3 eye(0.0f, 0.0f, 3.0f); // where the camera is
  glm::vec3 target(0.0f, 0.0f, 0.0f); // where the camera is pointing
  glm::vec3 up(0.0f, 1.0f, 0.0f); // what way is up (y = 1)
  
  // view matrix stores where the camera currently is and what its looking at
  glm::mat4 view = glm::lookAt(eye, target, up);
  // lookAt takes three vectors (eye, target, up)
  
  // perspective matrix deals with converting 3d coordinates to 2d output (to screen)
  glm::mat4 perspective = glm::perspective(glm::radians(45.0f), 1280.0f / 720.0f, 0.1f, 100.0f);
  // perspective takes in fov, aspect ratio, when to clip a close object, when to clip a far object

  // this is the main render loop that runs 60 times per second (60FPS)
  while (!glfwWindowShouldClose(window)) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // sets the background to chose colour

    // rendering the triangle
    basic_shader.use();
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
