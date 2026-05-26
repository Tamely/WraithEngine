#include "Core/ApplicationModules.h"

#include "Core/Application.h"

namespace Axiom {
std::string_view WindowEventsModule::GetName() const {
  return "Core.WindowEvents";
}

bool WindowEventsModule::Initialize(Application &App) {
  (void)App;
  return true;
}

void WindowEventsModule::Update(const ModuleUpdateContext &Context) {
  if (Context.Phase != ModuleUpdatePhase::FrameStart) {
    return;
  }

  if (Window *Window = Context.App.GetWindow(); Window != nullptr) {
    Window->PollEvents();
  }
}

void WindowEventsModule::Shutdown(Application &App) { (void)App; }

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
