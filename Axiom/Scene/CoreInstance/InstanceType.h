#pragma once

#include <cstdint>

namespace Axiom {
enum class InstanceType : uint16_t {
  Instance,
  DataModel,
  SceneFolder,
  SceneMeshObject,
  SceneLight,
  SceneCamera,
  SceneActor,
};
} // namespace Axiom
