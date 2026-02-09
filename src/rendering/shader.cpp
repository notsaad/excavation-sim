#include "shader.h"

#include <fstream>
#include <glad/gl.h>

#include <GLFW/glfw3.h>

#include <cstdlib>
#include <iostream>
#include <sstream>

std::string readFile(const std::string &file_path) {
  std::ifstream file(file_path);

  if (!file.is_open()) {
    std::cerr << "failed to open file: " << file_path << std::endl;
    return "";
  }

  std::stringstream buffer;
  // Read entire file into stringstream
  buffer << file.rdbuf();

  // Convert to string;
  return buffer.str()
}

Shader::Shader(const std::string &vertex_path,
               const std::string &fragment_path) {

  std::string vertexCode = readFile(vertex_path);
  std::string fragmentCode = readFile(fragment_path);

  // render both shaders
  GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
  GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
}

Shader::~Shader() {};
