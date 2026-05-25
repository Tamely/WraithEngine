#pragma once

#include "Physics/PhysicsWorld.h"
#include "Session/EditorSession.h"

namespace Axiom {
class EditorPhysicsController {
public:
  explicit EditorPhysicsController(EditorSession &Session);

  void EnsurePhysicsWorldStarted();
  void StopPhysicsWorld();
  void StepRuntimePhysics(float DeltaTimeSeconds);

private:
  EditorSession &m_Session;
  std::unique_ptr<PhysicsWorld> m_PhysicsWorld;
};
} // namespace Axiom
