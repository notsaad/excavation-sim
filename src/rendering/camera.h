#pragma once

#include <glm/glm.hpp>

class Camera {
private:
  glm::vec3 position;
  glm::vec3 up;
  float yaw;   // mouse X movement
  float pitch; // mouse Y movement
  glm::vec3 front;
  float speed;
  
  glm::vec3 updateFront(float yaw, float pitch);

public:
  Camera();
  void moveForward();
  void moveBackward();
  void moveRight();
  void moveLeft();
  void moveUp();
  void moveDown();
  glm::mat4 getViewMatrix();
};
