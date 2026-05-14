#pragma once

#include "Session/EditorSession.h"

#include <memory>
#include <string>
#include <vector>

namespace Axiom {

struct PhysicsTransformUpdate {
  std::string ObjectId;
  EditorTransformDetails WorldTransform;
};

class PhysicsWorld final {
public:
  PhysicsWorld();
  ~PhysicsWorld();

  bool IsAvailable() const;
  bool IsRunning() const;

  void Start(const EditorSceneState &Scene);
  void Stop();
  std::vector<PhysicsTransformUpdate> Step(float DeltaTimeSeconds);

private:
  struct Impl;
  std::unique_ptr<Impl> m_Impl;
};

} // namespace Axiom
