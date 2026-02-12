#include "camera.h"
#include "glm/geometric.hpp"
#include <glm/gtc/matrix_transform.hpp>

#include <glm/glm.hpp>

// private function
void Camera::updateFront() {
    float x, y, z;
    x = glm::cos(yaw) * glm::cos(pitch);
    y = glm::sin(pitch);
    z = glm::sin(yaw) * glm::cos(pitch);
    
    glm::vec3 new_front(x, y, z);
    front = glm::normalize(new_front);
}

Camera::Camera() {
  position = glm::vec3(0.0f, 0.0f, 3.0f);
  updateFront();
}

void Camera::moveForward() {
    position += (front * speed);
}

void Camera::moveBackward() {
    position -= (front * speed);
}

void Camera::moveRight() {
    glm::vec3 right = glm::normalize(glm::cross(front, up));
    position += (right * speed);
}

void Camera::moveLeft() {
    glm::vec3 right = glm::normalize(glm::cross(front, up));
    position -= (right * speed);
}

void Camera::moveUp() {
    position += (up * speed);
}

void Camera::moveDown() {
    position -= (up * speed);
}

glm::mat4 Camera::getViewMatrix() {
    glm::mat4 view = glm::lookAt(position, position + front, up);
    return view;
}
