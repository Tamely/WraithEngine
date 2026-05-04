#include "Renderer/Camera.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/trigonometric.hpp>

namespace Axiom {
void Camera::LookAt(const glm::vec3 &Position, const glm::vec3 &Target,
                    const glm::vec3 &Up) {
  m_Position = Position;
  m_View = glm::lookAt(Position, Target, Up);
}

void Camera::SetPerspective(float VerticalFovDegrees, float AspectRatio,
                            float NearPlane, float FarPlane) {
  m_Projection = glm::perspective(glm::radians(VerticalFovDegrees), AspectRatio,
                                  NearPlane, FarPlane);
  m_Projection[1][1] *= -1.0f;
}
} // namespace Axiom
