#include "shader.h"

#include <fstream>
#include <glad/gl.h>

#include <GLFW/glfw3.h>

#include <cstdlib>
#include <iostream>
#include <sstream>

std::string Shader::readFile(const std::string &file_path) {
  std::ifstream file(file_path);

  if (!file.is_open()) {
    std::cerr << "failed to open file: " << file_path << std::endl;
    return "";
  }

  std::stringstream buffer;
  // Read entire file into stringstream
  buffer << file.rdbuf();

  // convert to string
  return buffer.str();
}

Shader::Shader(const std::string &vertex_path,
               const std::string &fragment_path) {

  std::string vertexCode = readFile(vertex_path);
  std::string fragmentCode = readFile(fragment_path);

  // convert to c style strings
  const char *vertexSource = vertexCode.c_str();
  const char *fragmentSource = fragmentCode.c_str();

  // render both shaders
  GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
  GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

  // link the shaders to their source code
  glShaderSource(vertexShader, 1, &vertexSource, nullptr);
  glShaderSource(fragmentShader, 1, &fragmentSource, nullptr);

  // compile shaders
  glCompileShader(vertexShader);
  glCompileShader(fragmentShader);

  int vertexSuccess, fragmentSuccess;
  
  glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &vertexSuccess);
  glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &fragmentSuccess);
  if (!vertexSuccess || !fragmentSuccess)
    std::cout << "error occurred linking\n";

  // make a program for the gpu to execute
  programID = glCreateProgram();

  glAttachShader(programID, vertexShader);
  glAttachShader(programID, fragmentShader);

  glLinkProgram(programID);
  
  // linked to program, can delete shaders
  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);
}

Shader::~Shader() {
    glDeleteProgram(programID);
}

void Shader::use() {
    glUseProgram(programID);
}
