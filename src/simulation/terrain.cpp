#include "terrain.h"
#include <unordered_set>

// private functions
float Terrain::heightFunction(size_t r, size_t c) {
  float x = static_cast<float>(r);
  float z = static_cast<float>(c);

  // multiple octaves of sine waves to approximate natural terrain
  float h = 0.0f;
  h += 0.3f * std::sin(x * 0.05f) * std::cos(z * 0.07f);                // large rolling hills
  h += 0.15f * std::sin(x * 0.15f + 1.3f) * std::sin(z * 0.12f + 0.7f); // medium undulation
  h += 0.05f * std::sin(x * 0.4f + 2.7f) * std::cos(z * 0.35f + 1.2f);  // small bumps
  return h;
}

glm::vec3 Terrain::normalComputation(size_t i, size_t j) {
  float left = i == 0 ? this->heights[i][j] : this->heights[i - 1][j];
  float right = i == this->GRID_SIZE - 1 ? this->heights[i][j] : this->heights[i + 1][j];
  float up = j == this->GRID_SIZE - 1 ? this->heights[i][j] : this->heights[i][j + 1];
  float down = j == 0 ? this->heights[i][j] : this->heights[i][j - 1];

  glm::vec3 tangentX = glm::vec3(2 * SPACING, right - left, 0);
  glm::vec3 tangentZ = glm::vec3(0, up - down, 2 * SPACING);

  glm::vec3 normal = glm::normalize(cross(tangentZ, tangentX));
  return normal;
}

void Terrain::updateNeighbours(size_t r, size_t c, bool dig, float dt) {
  // the amount to change neighbouring terrain cells by
  float delta = dig ? -0.5f : 0.5f;
  delta *= dt;
  if (r != 0) {
    heights[r - 1][c] += delta;
  }
  if (c != 0) {
    heights[r][c - 1] += delta;
  }
  if (r != this->GRID_SIZE - 1) {
    heights[r + 1][c] += delta;
  }
  if (c != this->GRID_SIZE - 1) {
    heights[r][c + 1] += delta;
  }
}

void Terrain::stabilizeSoil() {
  static constexpr std::pair<int, int> directions[] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
  for (int i = 0; i < this->GRID_SIZE; ++i) {
    for (int j = 0; j < this->GRID_SIZE; ++j) {
      for (const auto &[x, y] : directions) {
        if (i + x >= 0 && i + x < this->GRID_SIZE && j + y >= 0 && j + y < this->GRID_SIZE) {
          int ni = i + x, nj = j + y;
          if (this->heights[i][j] - this->heights[ni][nj] > MAX_DIFF) {
            this->modifiedVertices.insert(static_cast<size_t>(i) * GRID_SIZE + static_cast<size_t>(j));
            this->heights[i][j] -= 0.005f;
            this->heights[ni][nj] += 0.005f;
          }
        }
      }
    }
  }
}

void Terrain::rebuildVertices() {
  // expand dirty set to include neighbours (their normals depend on adjacent heights)
  std::unordered_set<size_t> updateIndices;
  for (size_t idx : modifiedVertices) {
    size_t r = idx / GRID_SIZE;
    size_t c = idx % GRID_SIZE;
    for (int dr = -1; dr <= 1; ++dr) {
      for (int dc = -1; dc <= 1; ++dc) {
        int nr = static_cast<int>(r) + dr;
        int nc = static_cast<int>(c) + dc;
        if (nr >= 0 && nr < GRID_SIZE && nc >= 0 && nc < GRID_SIZE) {
          updateIndices.insert(static_cast<size_t>(nr) * GRID_SIZE + static_cast<size_t>(nc));
        }
      }
    }
  }

  // update only dirty vertices in-place
  for (size_t idx : updateIndices) {
    size_t i = idx / GRID_SIZE;
    size_t j = idx % GRID_SIZE;
    glm::vec3 n = normalComputation(i, j);
    size_t offset = idx * 6;
    vertices[offset] = i * SPACING;
    vertices[offset + 1] = heights[i][j];
    vertices[offset + 2] = j * SPACING;
    vertices[offset + 3] = n.x;
    vertices[offset + 4] = n.y;
    vertices[offset + 5] = n.z;
  }

  // upload only the affected range to the GPU
  auto [minIt, maxIt] = std::minmax_element(updateIndices.begin(), updateIndices.end());
  size_t minIdx = *minIt;
  size_t maxIdx = *maxIt;
  size_t byteOffset = minIdx * 6 * sizeof(float);
  size_t byteSize = (maxIdx - minIdx + 1) * 6 * sizeof(float);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferSubData(GL_ARRAY_BUFFER, byteOffset, byteSize, &vertices[minIdx * 6]);
}

// public functions
Terrain::Terrain() {
  // set initial heights in terrain array
  for (size_t i = 0; i < this->GRID_SIZE; ++i) {
    for (size_t j = 0; j < this->GRID_SIZE; ++j) {
      this->heights[i][j] = heightFunction(i, j);
    }
  }

  // set vectors
  for (size_t i = 0; i < this->GRID_SIZE; ++i) {
    for (size_t j = 0; j < this->GRID_SIZE; ++j) {
      glm::vec3 n = normalComputation(i, j);
      vertices.insert(vertices.end(),
                      {i * SPACING, this->heights[i][j], j * SPACING, n.x, n.y, n.z});
    }
  }

  for (size_t i = 0; i < this->GRID_SIZE - 1; ++i) {
    for (size_t j = 0; j < this->GRID_SIZE - 1; ++j) {
      unsigned int top_left = i * this->GRID_SIZE + j;
      unsigned int top_right = i * this->GRID_SIZE + (j + 1);
      unsigned int bottom_left = (i + 1) * this->GRID_SIZE + j;
      unsigned int bottom_right = (i + 1) * this->GRID_SIZE + (j + 1);

      connections.insert(connections.end(), {top_left, bottom_left, bottom_right});
      connections.insert(connections.end(), {top_left, bottom_right, top_right});
    }
  }

  glGenVertexArrays(1, &this->VAO);
  glBindVertexArray(this->VAO);
  glGenBuffers(1, &this->EBO);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->EBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, connections.size() * sizeof(unsigned int),
               connections.data(), GL_STATIC_DRAW);
  // memory chunks fed from cpu to gpu to handle graphics rendering
  glGenBuffers(1, &this->VBO);
  // static draw tells the GPU data is set once and used multiple times
  glBindBuffer(GL_ARRAY_BUFFER, this->VBO);
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
}

Terrain::~Terrain() {}

void Terrain::draw(Shader &shader, const glm::mat4 &vp) {
  shader.uniformInfo("mvp", vp);
  shader.uniformInfo("objectColour", glm::vec3(0.55f, 0.36f, 0.2f));
  glBindVertexArray(VAO);
  glDrawElements(GL_TRIANGLES, connections.size(), GL_UNSIGNED_INT, 0);
}

void Terrain::modify(size_t row, size_t col, bool dig, float dt) {
  float delta = dig ? -1.0f : 1.0f;
  delta *= dt;
  heights[row][col] += delta;
  updateNeighbours(row, col, dig, dt);

  size_t prevSize = this->modifiedVertices.size();
  for (size_t iter = 0; iter < 10; ++iter) {
      stabilizeSoil();
      if (prevSize == this->modifiedVertices.size()) {
      break;
    }
  }

  this->modifiedVertices.insert(row * GRID_SIZE + col);
  if (row > 0) this->modifiedVertices.insert((row - 1) * GRID_SIZE + col);
  if (row < GRID_SIZE - 1) this->modifiedVertices.insert((row + 1) * GRID_SIZE + col);
  if (col > 0) this->modifiedVertices.insert(row * GRID_SIZE + (col - 1));
  if (col < GRID_SIZE - 1) this->modifiedVertices.insert(row * GRID_SIZE + (col + 1));

  rebuildVertices();
  this->modifiedVertices.clear();
}

std::optional<float> Terrain::getHeight(size_t row, size_t col) {
  if (row >= 0 && col >= 0 && row < this->GRID_SIZE && col < this->GRID_SIZE) {
    return this->heights[row][col];
  }
  return std::nullopt;
}

std::pair<size_t, size_t> Terrain::worldToGrid(float x, float z) {
  std::pair<size_t, size_t> coordinates = {0, 0};
  coordinates.first =
      static_cast<size_t>(std::clamp(static_cast<int>(x / this->SPACING), 0, this->GRID_SIZE - 1));
  coordinates.second =
      static_cast<size_t>(std::clamp(static_cast<int>(z / this->SPACING), 0, this->GRID_SIZE - 1));
  return coordinates;
}
