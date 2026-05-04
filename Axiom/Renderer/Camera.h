#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace Axiom {
class Camera {
public:
  Camera() = default;

  void LookAt(const glm::vec3 &Position, const glm::vec3 &Target,
              const glm::vec3 &Up = glm::vec3(0.0f, 1.0f, 0.0f));
  void SetPerspective(float VerticalFovDegrees, float AspectRatio,
                      float NearPlane, float FarPlane);

  const glm::vec3 &GetPosition() const { return m_Position; }
  const glm::mat4 &GetViewMatrix() const { return m_View; }
  const glm::mat4 &GetProjectionMatrix() const { return m_Projection; }
  glm::mat4 GetViewProjectionMatrix() const { return m_Projection * m_View; }

private:
  glm::vec3 m_Position{0.0f, 0.0f, 0.0f};
  glm::mat4 m_View{1.0f};
  glm::mat4 m_Projection{1.0f};
};
} // namespace Axiom
