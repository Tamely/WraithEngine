#pragma once

#include "Session/SessionTypes.h"

#include <cstdint>
#include <string>
#include <vector>

#include <glm/vec3.hpp>

namespace Axiom {
struct RuntimePhysicsMaterial {
  float Friction{0.2f};
  float Restitution{0.0f};
};

struct RuntimeSceneBodyState {
  std::string ObjectId;
  EditorTransformDetails WorldTransform;
  EditorPhysicsBodyType BodyType{EditorPhysicsBodyType::None};
  EditorPhysicsColliderType ColliderType{EditorPhysicsColliderType::None};
  glm::vec3 BoxHalfExtents{0.5f, 0.5f, 0.5f};
  float SphereRadius{0.5f};
  float Mass{1.0f};
  uint32_t MaterialIndex{0};
};

struct RuntimeSceneState {
  std::vector<RuntimePhysicsMaterial> Materials;
  std::vector<RuntimeSceneBodyState> Bodies;
};
} // namespace Axiom
