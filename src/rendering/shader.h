#pragma once

#include <string>

class Shader {
private:
    std::string readFile(const std::string& file_path);
    
public:
    Shader(const std::string& vertex_path, const std::string& fragment_path);
    ~Shader();
}
