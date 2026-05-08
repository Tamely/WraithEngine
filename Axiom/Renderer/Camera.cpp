#include "Renderer/Camera.h"

#include <glm/common.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <cmath>

namespace Axiom {
void Camera::LookAt(const glm::vec3 &Position, const glm::vec3 &Target,
                    const glm::vec3 &Up) {
  m_Position = Position;
  m_Forward = glm::normalize(Target - Position);
  m_Right = glm::normalize(glm::cross(m_Forward, Up));
  m_Up = glm::normalize(glm::cross(m_Right, m_Forward));

  m_PitchDegrees = glm::degrees(glm::asin(glm::clamp(m_Forward.y, -1.0f, 1.0f)));
  m_YawDegrees = glm::degrees(glm::atan(m_Forward.z, m_Forward.x));
  UpdateOrientationVectors();
  UpdateViewMatrix();
}

void Camera::SetPerspective(float VerticalFovDegrees, float AspectRatio,
                            float NearPlane, float FarPlane) {
  m_Projection = glm::perspective(glm::radians(VerticalFovDegrees), AspectRatio,
                                  NearPlane, FarPlane);
  m_Projection[1][1] *= -1.0f;
}

void Camera::SetPosition(const glm::vec3 &Position) {
  m_Position = Position;
  UpdateViewMatrix();
}

void Camera::SetRotation(float YawDegrees, float PitchDegrees) {
  m_YawDegrees = YawDegrees;
  m_PitchDegrees = glm::clamp(PitchDegrees, -89.0f, 89.0f);
  UpdateOrientationVectors();
  UpdateViewMatrix();
}

void Camera::MoveLocal(const glm::vec3 &LocalOffset) {
  m_Position += m_Right * LocalOffset.x;
  m_Position += m_Up * LocalOffset.y;
  m_Position += m_Forward * LocalOffset.z;
  UpdateViewMatrix();
}

void Camera::MoveWorld(const glm::vec3 &WorldOffset) {
  m_Position += WorldOffset;
  UpdateViewMatrix();
}

void Camera::UpdateOrientationVectors() {
  const float YawRadians = glm::radians(m_YawDegrees);
  const float PitchRadians = glm::radians(m_PitchDegrees);

  glm::vec3 Forward;
  Forward.x = std::cos(YawRadians) * std::cos(PitchRadians);
  Forward.y = std::sin(PitchRadians);
  Forward.z = std::sin(YawRadians) * std::cos(PitchRadians);

  m_Forward = glm::normalize(Forward);
  m_Right = glm::normalize(glm::cross(m_Forward, glm::vec3(0.0f, 1.0f, 0.0f)));
  m_Up = glm::normalize(glm::cross(m_Right, m_Forward));
}

void Camera::UpdateViewMatrix() {
  m_View = glm::lookAt(m_Position, m_Position + m_Forward, m_Up);
}
} // namespace Axiom
