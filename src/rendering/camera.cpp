#include "camera.h"
#include "glm/geometric.hpp"
#include <glm/gtc/matrix_transform.hpp>

#include <glm/glm.hpp>

void Camera::updateFront() {
  float x, y, z;
  x = glm::cos(yaw) * glm::cos(pitch);
  y = glm::sin(pitch);
  z = glm::sin(yaw) * glm::cos(pitch);

  glm::vec3 new_front(x, y, z);
  front = glm::normalize(new_front);
}

Camera::Camera() {
  position = glm::vec3(1.5f, 2.0f, -7.0f);
  updateFront();
}

void Camera::moveForward(float dt) { position += (front * (speed * dt)); }

void Camera::moveBackward(float dt) { position -= (front * (speed * dt)); }

void Camera::moveRight(float dt) {
  glm::vec3 right = glm::normalize(glm::cross(front, up));
  position += (right * (speed * dt));
}

void Camera::moveLeft(float dt) {
  glm::vec3 right = glm::normalize(glm::cross(front, up));
  position -= (right * (speed * dt));
}

void Camera::moveUp(float dt) { position += (up * (speed * dt)); }

void Camera::moveDown(float dt) { position -= (up * (speed * dt)); }

void Camera::look(float xOffset, float yOffset) {
  yaw += (xOffset * 0.002f);

  pitch += (yOffset * 0.002f);
  // this is clamping the pitch
  // this is done bc if pitch = +-90 degrees then you look straight up
  // and this breaks glm::lookAt as it uses cross product and
  // both vectors would be parallel
  if (pitch > glm::radians(89.0f)) {
    pitch = glm::radians(89.0f);
  } else if (pitch < glm::radians(-89.0f)) {
    pitch = glm::radians(-89.0f);
  }

  updateFront();
}

glm::mat4 Camera::getViewMatrix() {
  glm::mat4 view = glm::lookAt(position, position + front, up);
  return view;
}
