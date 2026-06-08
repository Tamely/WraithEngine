#pragma once

#include "Core/IModule.h"
#include "Jobs/JobSystem.h"

namespace Axiom {
class RendererFrameModule final : public IModule {
public:
  [[nodiscard]] std::string_view GetName() const override;
  bool Initialize(Application &App) override;
  void Update(const ModuleUpdateContext &Context) override;
  void Shutdown(Application &App) override;

private:
  void UpdateSerial(const ModuleUpdateContext &Context);
  void UpdateTaskGraph(const ModuleUpdateContext &Context);
  void ResetTaskGraph();

  Jobs::JobHandle m_BeginFrameJob;
  Jobs::JobHandle m_RenderJob;
  Jobs::JobHandle m_EndFrameJob;
  bool m_UseFrameTaskGraph{false};
};
} // namespace Axiom
