#include <array>
#include <glad/gl.h>

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>                  // mat4, vec3, vec4 (core types)
#include <glm/gtc/matrix_transform.hpp> // lookAt, perspective (transformations)
#include <glm/gtc/type_ptr.hpp>         // value_ptr (convert to OpenGL pointer)

#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/trigonometric.hpp"
#include "rendering/camera.h"
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
  const float SPACING = 0.1f;
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

  std::vector<float> vertices;
  std::array<std::array<int, 32>, 32> heights{};
  std::vector<unsigned int> connections;

  for (size_t i = 0; i < 32; ++i) {
    for (size_t j = 0; j < 32; ++j) {
      vertices.insert(vertices.end(),
                      {i * SPACING, static_cast<float>(heights[i][j]), j * SPACING});
    }
  }

  for (size_t i = 0; i < 31; ++i) {
    for (size_t j = 0; j < 31; ++j) {
      unsigned int top_left = i * heights.size() + j;
      unsigned int top_right = i * heights.size() + (j + 1);
      unsigned int bottom_left = (i + 1) * heights.size() + j;
      unsigned int bottom_right = (i + 1) * heights.size() + (j + 1);

      connections.insert(connections.end(), {top_left, bottom_left, bottom_right});
      connections.insert(connections.end(), {top_left, bottom_right, top_right});
    }
  }

  GLuint VBO; // vertex buffer object
  GLuint VAO; // vertex array object (how to read the vbo)
  GLuint EBO; // element buffer object
  glGenVertexArrays(1, &VAO);
  glBindVertexArray(VAO);
  glGenBuffers(1, &EBO);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, connections.size() * sizeof(unsigned int),
               connections.data(), GL_STATIC_DRAW);
  // memory chunks fed from cpu to gpu to handle graphics rendering
  glGenBuffers(1, &VBO);
  // static draw tells the GPU data is set once and used multiple times
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  // copies user defined data into bound buffer
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
  // to describe the vertex layout for vertices
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
  glEnableVertexAttribArray(0);

  // using shader class and giving path to shader code
  Shader basic_shader("shaders/basic.vert", "shaders/basic.frag");

  // model matrix (4x4 identity matrix)
  glm::mat4 model = glm::mat4(1.0f);
  // this includes the view matrix
  Camera camera;
  // perspective matrix deals with converting 3d coordinates to 2d output (to screen)
  glm::mat4 perspective = glm::perspective(glm::radians(45.0f), 1280.0f / 720.0f, 0.1f, 100.0f);
  // perspective takes in fov, aspect ratio, when to clip a close object, when to clip a far object

  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // TODO: visual debugging

  // this is the main render loop that runs 60 times per second (60FPS)
  while (!glfwWindowShouldClose(window)) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // sets the background to chose colour

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
      camera.moveForward();
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
      camera.moveLeft();
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
      camera.moveBackward();
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
      camera.moveRight();
    }
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
      camera.moveUp();
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
      camera.moveDown();
    }

    glm::mat4 mvp = perspective * camera.getViewMatrix() * model;

    // rendering the triangle
    basic_shader.use();
    basic_shader.uniformInfo("mvp", mvp);

    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, connections.size(), GL_UNSIGNED_INT, 0);

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
