#include "Renderer/RendererFrameModule.h"

#include "Core/Application.h"
#include "Renderer/Renderer.h"

#include <array>
#include <span>

namespace Axiom {
std::string_view RendererFrameModule::GetName() const {
  return "Core.RendererFrame";
}

bool RendererFrameModule::Initialize(Application &App) {
  m_UseFrameTaskGraph = App.IsFrameTaskGraphEnabled();
  ResetTaskGraph();
  return true;
}

void RendererFrameModule::Update(const ModuleUpdateContext &Context) {
  if (m_UseFrameTaskGraph) {
    UpdateTaskGraph(Context);
    return;
  }

  UpdateSerial(Context);
}

void RendererFrameModule::UpdateSerial(const ModuleUpdateContext &Context) {
  switch (Context.Phase) {
  case ModuleUpdatePhase::FrameStart:
    Context.App.GetRenderer().SetCpuFrameTime(Context.DeltaTimeSeconds *
                                              1000.0f);
    break;
  case ModuleUpdatePhase::RenderBegin:
    Context.App.GetRenderer().BeginFrame();
    break;
  case ModuleUpdatePhase::Render:
    Context.App.GetRenderer().Render();
    break;
  case ModuleUpdatePhase::RenderEnd:
    Context.App.GetRenderer().EndFrame();
    break;
  case ModuleUpdatePhase::ImGuiRender:
    break;
  }
}

void RendererFrameModule::UpdateTaskGraph(const ModuleUpdateContext &Context) {
  Renderer &Renderer = Context.App.GetRenderer();
  switch (Context.Phase) {
  case ModuleUpdatePhase::FrameStart:
    ResetTaskGraph();
    Renderer.SetCpuFrameTime(Context.DeltaTimeSeconds * 1000.0f);
    break;
  case ModuleUpdatePhase::RenderBegin:
    m_BeginFrameJob = Jobs::ScheduleJob([&Renderer]() { Renderer.BeginFrame(); });
    Jobs::Wait(m_BeginFrameJob);
    break;
  case ModuleUpdatePhase::Render: {
    std::array<Jobs::JobHandle, 1> Dependencies = {m_BeginFrameJob};
    m_RenderJob = Jobs::ScheduleJobAfter(
        [&Renderer]() { Renderer.Render(); },
        std::span<Jobs::JobHandle>(Dependencies));
    break;
  }
  case ModuleUpdatePhase::ImGuiRender:
    break;
  case ModuleUpdatePhase::RenderEnd: {
    std::array<Jobs::JobHandle, 1> Dependencies = {m_RenderJob};
    m_EndFrameJob = Jobs::ScheduleJobAfter(
        [&Renderer]() { Renderer.EndFrame(); },
        std::span<Jobs::JobHandle>(Dependencies));
    Jobs::Wait(m_EndFrameJob);
    break;
  }
  }
}

void RendererFrameModule::ResetTaskGraph() {
  m_BeginFrameJob = {};
  m_RenderJob = {};
  m_EndFrameJob = {};
}

void RendererFrameModule::Shutdown(Application &App) {
  (void)App;
  ResetTaskGraph();
}
} // namespace Axiom
