#pragma once

#include <array>
#include <cstdlib>
#include <glm/glm.hpp>
#include <optional>
#include <unordered_set>
#include <vector>

// FIXME
#include "../rendering/shader.h"
#include "glad/gl.h"

class Terrain {
private:
  static constexpr float SPACING = 0.1f;
  // SPACING * tan(33) to determine the angle of repose for soil
  static constexpr float MAX_DIFF = 0.065f;
  static constexpr int GRID_SIZE = 64;
  std::array<std::array<float, GRID_SIZE>, GRID_SIZE> heights{};
  std::vector<float> vertices;
  std::vector<unsigned int> connections;
  std::unordered_set<size_t> modifiedVertices;
  GLuint VBO; // vertex buffer object
  GLuint VAO; // vertex array object (how to read the vbo)
  GLuint EBO; // element buffer object

  float heightFunction(size_t r, size_t c);
  glm::vec3 normalComputation(size_t i, size_t j);
  void updateNeighbours(size_t r, size_t c, bool dig, float dt);
  void stabilizeSoil();
  void rebuildVertices();

public:
  Terrain();
  ~Terrain();
  void draw(Shader &, const glm::mat4 &vp);
  void modify(size_t row, size_t col, bool dig, float dt);
  std::optional<float> getHeight(size_t row, size_t col);
  std::pair<size_t, size_t> worldToGrid(float x, float z);
};
