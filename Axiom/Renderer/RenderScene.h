#pragma once

#include "Renderer/Mesh.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <optional>
#include <string>
#include <vector>

namespace Axiom {
class Camera;

enum class GizmoMode { Translate, Scale, Rotate };

struct GizmoOverlayData {
  glm::vec3 WorldPosition{0.0f};
  float Scale{0.5f};
  int HoveredAxis{-1};
  GizmoMode Mode{GizmoMode::Translate};
};

struct DirectionalLight {
  glm::vec3 Color{1.0f};
  float Intensity{1.0f};
  glm::vec3 Direction{0.35f, 0.7f, 0.2f}; // world-space, need not be normalized
};

struct LightBillboardOverlay {
  std::string ObjectId;
  glm::vec3 WorldPosition{0.0f};
  glm::vec4 Color{1.0f};
  float PixelSize{48.0f};
};

class RenderScene {
public:
  void Reset();

  const Camera *ActiveCamera{nullptr};
  glm::vec4 BackgroundColor{1.0f, 0.0f, 0.0f, 1.0f};
  std::vector<RenderMeshSubmission> Submissions;
  std::optional<GizmoOverlayData> GizmoOverlay;
  std::optional<DirectionalLight> Sun;
  std::vector<LightBillboardOverlay> LightBillboards;
  glm::vec3 SkyboxColorTop{0.08f, 0.09f, 0.14f};
  glm::vec3 SkyboxColorBottom{0.14f, 0.24f, 0.38f};
};
} // namespace Axiom
