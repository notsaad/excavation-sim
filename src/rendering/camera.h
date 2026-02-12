#pragma once

#include <glm/glm.hpp>

class Camera {
private:
  glm::vec3 position;
  glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
  // initial horizontal rotation (-90deg means camera pointing down the -Z axis)
  float yaw = glm::radians(90.0f);   // mouse X movement
  // initial vertical tilt
  float pitch = 0.0f; // mouse Y movement
  glm::vec3 front;
  float speed = 0.05f;
  
  void updateFront();

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
