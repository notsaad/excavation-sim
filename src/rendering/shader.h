#pragma once

#include <glm/glm.hpp>
#include <string>

class Shader {
private:
  unsigned int programID;
  std::string readFile(const std::string &file_path);

public:
  Shader(const std::string &vertex_path, const std::string &fragment_path);
  ~Shader();
  void use();
  void uniformInfo(const std::string &uniform, const glm::mat4 &matrix);
};
