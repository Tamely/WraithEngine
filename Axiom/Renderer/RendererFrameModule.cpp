#include "Renderer/RendererFrameModule.h"

#include "Core/Application.h"
#include "Renderer/Renderer.h"

namespace Axiom {
std::string_view RendererFrameModule::GetName() const {
  return "Core.RendererFrame";
}

bool RendererFrameModule::Initialize(Application &App) {
  (void)App;
  return true;
}

void RendererFrameModule::Update(const ModuleUpdateContext &Context) {
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

void RendererFrameModule::Shutdown(Application &App) { (void)App; }
} // namespace Axiom
