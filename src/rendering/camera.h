#pragma once

#include <glm/glm.hpp>

class Camera {
private:
  glm::vec3 position;
  glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
  // initial horizontal rotation (-90deg means camera pointing down the -Z axis)
  float yaw = glm::radians(90.0f); // mouse X movement
  // initial vertical tilt
  float pitch = 0.0f; // mouse Y movement
  glm::vec3 front;
  float speed = 5.0f;

  void updateFront();

public:
  Camera();
  void moveForward(float dt);
  void moveBackward(float dt);
  void moveRight(float dt);
  void moveLeft(float dt);
  void moveUp(float dt);
  void moveDown(float dt);
  void look(float xOffset, float yOffset);
  glm::mat4 getViewMatrix();
};
