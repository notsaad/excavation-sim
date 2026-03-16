#include "terrain.h"

// private functions
float Terrain::heightFunction(size_t r, size_t c) {
  return 0.15f * std::sin(r * 0.4f) + 0.1f * std::sin(c * 0.6f);
}

glm::vec3 Terrain::normalComputation(size_t i, size_t j) {
  float left = i == 0 ? this->heights[i][j] : this->heights[i - 1][j];
  float right = i == this->GRID_SIZE -1 ? this->heights[i][j] : this->heights[i + 1][j];
  float up = j == this->GRID_SIZE -1 ? this->heights[i][j] : this->heights[i][j + 1];
  float down = j == 0 ? this->heights[i][j] : this->heights[i][j - 1];

  glm::vec3 tangentX = glm::vec3(2 * SPACING, right - left, 0);
  glm::vec3 tangentZ = glm::vec3(0, up - down, 2 * SPACING);

  glm::vec3 normal = glm::normalize(cross(tangentZ, tangentX));
  return normal;
}

void Terrain::updateNeighbours(size_t r, size_t c, bool dig) {
  // the amount to change neighbouring terrain cells by
  float delta = dig ? -0.005f : 0.005f;
  if (r != 0) {
    heights[r - 1][c] += delta;
  }
  if (c != 0) {
    heights[r][c - 1] += delta;
  }
  if (r != this->GRID_SIZE -1) {
    heights[r + 1][c] += delta;
  }
  if (c != this->GRID_SIZE -1) {
    heights[r][c + 1] += delta;
  }
}

bool Terrain::stabilizeSoil() {
  bool stable = true;
  static constexpr std::pair<int, int> directions[] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
  for (int i = 0; i < this->GRID_SIZE; ++i) {
    for (int j = 0; j < this->GRID_SIZE; ++j) {
      for (const auto &[x, y] : directions) {
        if (i + x >= 0 && i + x < this->GRID_SIZE && j + y >= 0 && j + y < this->GRID_SIZE) {
          int ni = i + x, nj = j + y;
          if (this->heights[i][j] - this->heights[ni][nj] > MAX_DIFF) {
            stable = false;
            this->heights[i][j] -= 0.005f;
            this->heights[ni][nj] += 0.005f;
          }
        }
      }
    }
  }
  return stable;
}

// TODO: this is currently unoptimized, but fine for now
void Terrain::rebuildVertices() {
    vertices.clear();
    for (size_t i = 0; i < this->GRID_SIZE; ++i) {
      for (size_t j = 0; j < this->GRID_SIZE; ++j) {
        // the normals hold the information for lighting / shading
        glm::vec3 n = normalComputation(i, j);
        // vertices holds the information for smoothing out the terrain properly (breaking the
        // terrain into triangles)
        vertices.insert(vertices.end(), {i * SPACING, this->heights[i][j], j * SPACING, n.x, n.y, n.z});
      }
    }
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

  for (size_t i = 0; i < this->GRID_SIZE -1; ++i) {
    for (size_t j = 0; j < this->GRID_SIZE -1; ++j) {
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

void Terrain::modify(size_t row, size_t col, bool dig) {
  float delta = dig ? -0.01f : 0.01f;
  heights[row][col] += delta;
  updateNeighbours(row, col, dig);

  for (size_t iter = 0; iter < 10; ++iter) {
    if (stabilizeSoil()) {
      break;
    }
  }

  rebuildVertices();

  // reupload updated vertices data to gpu
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW);
}

std::optional<float> Terrain::getHeight(size_t row, size_t col) {
  if (row >= 0 && col >= 0 && row < this->GRID_SIZE && col < this->GRID_SIZE) {
    return this->heights[row][col];
  }
  return std::nullopt;
}

std::pair<size_t, size_t> Terrain::worldToGrid(float x, float z){
    std::pair<size_t, size_t> coordinates = {0, 0};
    coordinates.first = static_cast<size_t>(std::clamp(static_cast<int>(x / this->SPACING), 0, this->GRID_SIZE -1));
    coordinates.second = static_cast<size_t>(std::clamp(static_cast<int>(z / this->SPACING), 0, this->GRID_SIZE -1));
    return coordinates;
}
