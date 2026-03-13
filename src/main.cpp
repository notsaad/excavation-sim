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
#include <algorithm>
#include <cstdlib>
#include <iostream>

constexpr float SPACING = 0.1f;

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

void cursorPosCallBack(GLFWwindow *window, double xpos, double ypos) {
  // static means variables persist across calls without being global
  static bool firstMouseMovement = 1;
  static double lastX = 0.0;
  static double lastY = 0.0;

  // this is because the first xpos/ypos could be anywhere on the screen
  // and we dont want crazy drastic movement to start
  if (firstMouseMovement) {
    lastX = xpos;
    lastY = ypos;
    firstMouseMovement = 0;
    return;
  }

  float xOffset = xpos - lastX;
  float yOffset = lastY - ypos;

  lastX = xpos;
  lastY = ypos;

  Camera *camera = static_cast<Camera *>(glfwGetWindowUserPointer(window));
  camera->look(xOffset, yOffset);
}

float height_function(size_t x, size_t y) {
  return 0.15f * std::sin(x * 0.4f) + 0.1f * std::sin(y * 0.6f);
}

void lower_neighbours(size_t r, size_t c, std::array<std::array<float, 32>, 32> &heights) {
    if (r != 0) {
        heights[r-1][c] -= 0.005f;
    }
    if (c != 0) {
        heights[r][c-1] -= 0.005f;
    }
    if (r != 31) {
        heights[r+1][c] -= 0.005f;
    }
    if (c != 31) {
        heights[r][c+1] -= 0.005f;
    }
}

glm::vec3 normal_computation(const std::array<std::array<float, 32>, 32> &heights, size_t i,
                             size_t j) {
  float left = i == 0 ? heights[i][j] : heights[i - 1][j];
  float right = i == 31 ? heights[i][j] : heights[i + 1][j];
  float up = j == 31 ? heights[i][j] : heights[i][j + 1];
  float down = j == 0 ? heights[i][j] : heights[i][j - 1];

  glm::vec3 tangentX = glm::vec3(2 * SPACING, right - left, 0);
  glm::vec3 tangentZ = glm::vec3(0, up - down, 2 * SPACING);

  glm::vec3 normal = glm::normalize(cross(tangentZ, tangentX));
  return normal;
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

  // hides cursor and locks it to window (for camera movement)
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  Camera camera;
  // so that you can access the camera in the cursor pos callback
  glfwSetWindowUserPointer(window, &camera);
  glfwSetCursorPosCallback(window, cursorPosCallBack);

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
  std::array<std::array<float, 32>, 32> terrain{};
  std::vector<unsigned int> connections;

  // clang-format off
  // each vertex: x, y, z, nx, ny, nz
  std::array<float, 144> cubeVertices = {
    // front face (+Z)
    -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
     0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
    -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
    // back face (-Z)
    -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
    -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
     0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
     0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
    // top face (+Y)
    -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
    -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
     0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
    // bottom face (-Y)
    -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
     0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
     0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
    -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
    // right face (+X)
     0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
     0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
     0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
     0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
    // left face (-X)
    -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
    -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
    -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
    -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
  };

  std::array<unsigned int, 36> cubeIndices = {
      0,  1,  2,  0,  2,  3,  // front
      4,  5,  6,  4,  6,  7,  // back
      8,  9,  10, 8,  10, 11, // top
      12, 13, 14, 12, 14, 15, // bottom
      16, 17, 18, 16, 18, 19, // right
      20, 21, 22, 20, 22, 23, // left
  };
  // clang-format on

  // set initial heights in terrain array
  for (size_t i = 0; i < 32; ++i) {
    for (size_t j = 0; j < 32; ++j) {
      terrain[i][j] = height_function(i, j);
    }
  }

  // set vectors
  for (size_t i = 0; i < 32; ++i) {
    for (size_t j = 0; j < 32; ++j) {
      glm::vec3 n = normal_computation(terrain, i, j);
      vertices.insert(vertices.end(), {i * SPACING, terrain[i][j], j * SPACING, n.x, n.y, n.z});
    }
  }

  // 32x32 grid filled with triangles
  for (size_t i = 0; i < 31; ++i) {
    for (size_t j = 0; j < 31; ++j) {
      unsigned int top_left = i * terrain.size() + j;
      unsigned int top_right = i * terrain.size() + (j + 1);
      unsigned int bottom_left = (i + 1) * terrain.size() + j;
      unsigned int bottom_right = (i + 1) * terrain.size() + (j + 1);

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
  // dynamic draw tells the gpu driver that this buffer will be updated frequently so allocate
  // memory to optimize for this
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW);

  // tells opengl how to read position data from the buffer
  // we have the vector and its normal vector both stored so there is two calls to read
  // one is standard vectors, the other is starting from the normals

  // position: attribute 0, offset 0
  // index, component #, data type, normalize?, stride, offset
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  // normal: attribute 1, offset 3 floats
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  GLuint bucketVAO, bucketVBO, bucketEBO;

  glGenVertexArrays(1, &bucketVAO);
  glBindVertexArray(bucketVAO);

  glGenBuffers(1, &bucketEBO);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bucketEBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, cubeIndices.size() * sizeof(unsigned int),
               cubeIndices.data(), GL_STATIC_DRAW);

  glGenBuffers(1, &bucketVBO);
  glBindBuffer(GL_ARRAY_BUFFER, bucketVBO);
  glBufferData(GL_ARRAY_BUFFER, cubeVertices.size() * sizeof(float), cubeVertices.data(),
               GL_DYNAMIC_DRAW);

  // position
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  // normals
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  // using shader class and giving path to shader code
  Shader basic_shader("shaders/basic.vert", "shaders/basic.frag");

  // model matrix (4x4 identity matrix)
  glm::mat4 model = glm::mat4(1.0f);
  // this includes the view matrix
  // perspective matrix deals with converting 3d coordinates to 2d output (to screen)
  glm::mat4 perspective = glm::perspective(glm::radians(45.0f), 1280.0f / 720.0f, 0.1f, 100.0f);
  // perspective takes in fov, aspect ratio, when to clip a close object, when to clip a far object

  glm::vec3 bucketPos(1.5f, 0.5f, -1.5f);

  // this is the main render loop that runs 60 times per second (60FPS)
  while (!glfwWindowShouldClose(window)) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // sets the background to chose colour

    // camera movement checks (wasd/shift/space)
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

    // bucket movement checks (arrow keys)
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
      bucketPos.x -= 0.02f;
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
      bucketPos.x += 0.02f;
    }
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
      bucketPos.z += 0.02f;
    }
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
      bucketPos.z -= 0.02f;
    }

    basic_shader.use();

    // terrain
    glm::mat4 terrainMvp = perspective * camera.getViewMatrix() * model;
    basic_shader.uniformInfo("mvp", terrainMvp);
    basic_shader.uniformInfo("objectColour", glm::vec3(0.55f, 0.36f, 0.2f));
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, connections.size(), GL_UNSIGNED_INT, 0);

    // bucket
    size_t bucketRow =
        static_cast<size_t>(std::clamp(static_cast<int>(bucketPos.x / SPACING), 0, 31));
    size_t bucketCol =
        static_cast<size_t>(std::clamp(static_cast<int>(bucketPos.z / SPACING), 0, 31));
    
    // also set the bucket's height to the current terrain height
    // to make it look like its 'digging'
    bucketPos.y = terrain[bucketRow][bucketCol];
    glm::mat4 bucketModel = glm::translate(glm::mat4(1.0f), bucketPos) *
                             glm::scale(glm::mat4(1.0f), glm::vec3(0.3f));
    glm::mat4 bucketMvp = perspective * camera.getViewMatrix() * bucketModel;
    basic_shader.uniformInfo("mvp", bucketMvp);
    basic_shader.uniformInfo("objectColour", glm::vec3(0.9f, 0.75, 0.2f));
    glBindVertexArray(bucketVAO);
    glDrawElements(GL_TRIANGLES, cubeIndices.size(), GL_UNSIGNED_INT, 0);

    // button to dig
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
      terrain[bucketRow][bucketCol] -= 0.01f;
      // lower the neighbours by a smaller amount to make the terrain make sense
      lower_neighbours(bucketRow, bucketCol, terrain);
      
      // TODO: this is currently unoptimized, but fine for now
      // rebuild the vertices because of the new terrain
      vertices.clear();
      for (size_t i = 0; i < 32; ++i) {
        for (size_t j = 0; j < 32; ++j) {
          // the normals hold the information for lighting / shading
          glm::vec3 n = normal_computation(terrain, i, j);
          // vertices holds the information for smoothing out the terrain properly (breaking the
          // terrain into triangles)
          vertices.insert(vertices.end(), {i * SPACING, terrain[i][j], j * SPACING, n.x, n.y, n.z});
        }
      }

      // reupload updated vertices data to gpu
      glBindBuffer(GL_ARRAY_BUFFER, VBO);
      glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(),
                   GL_DYNAMIC_DRAW);
    }

    // TODO: simulation step

    glfwSwapBuffers(window); // shows new frame
    glfwPollEvents();        // polls for actions
  }

  glfwDestroyWindow(window);
  glfwTerminate();
  return EXIT_SUCCESS;
}
