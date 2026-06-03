#pragma once

namespace Axiom {

class IEditorRuntimePhysicsController {
public:
  virtual ~IEditorRuntimePhysicsController() = default;

  virtual void EnsurePhysicsWorldStarted() = 0;
  virtual void StopPhysicsWorld() = 0;
  virtual void StepRuntimePhysics(float DeltaTimeSeconds) = 0;
};

} // namespace Axiom
