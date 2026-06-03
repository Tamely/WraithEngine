#pragma once

#include "Session/EditorRuntimePhysicsController.h"
#include "Physics/PhysicsWorld.h"
#include "Session/EditorSession.h"

namespace Axiom {
class EditorPhysicsController final : public IEditorRuntimePhysicsController {
public:
  explicit EditorPhysicsController(EditorSession &Session);

  void EnsurePhysicsWorldStarted() override;
  void StopPhysicsWorld() override;
  void StepRuntimePhysics(float DeltaTimeSeconds) override;

private:
  EditorSession &m_Session;
  std::unique_ptr<PhysicsWorld> m_PhysicsWorld;
};

void AttachEditorPhysicsController(EditorSession &Session);
} // namespace Axiom
