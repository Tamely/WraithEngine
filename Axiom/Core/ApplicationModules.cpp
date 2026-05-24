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

std::string_view LayerUpdateModule::GetName() const {
  return "Core.LayerUpdate";
}

bool LayerUpdateModule::Initialize(Application &App) {
  (void)App;
  return true;
}

void LayerUpdateModule::Update(const ModuleUpdateContext &Context) {
  if (Context.Phase != ModuleUpdatePhase::FrameStart) {
    return;
  }

  Context.App.ForEachLayer([](Layer *Layer) { Layer->OnUpdate(); });
}

void LayerUpdateModule::Shutdown(Application &App) { (void)App; }

std::string_view LayerRenderModule::GetName() const {
  return "Core.LayerRender";
}

bool LayerRenderModule::Initialize(Application &App) {
  (void)App;
  return true;
}

void LayerRenderModule::Update(const ModuleUpdateContext &Context) {
  if (Context.Phase == ModuleUpdatePhase::Render) {
    Context.App.ForEachLayer([](Layer *Layer) { Layer->OnRender(); });
    return;
  }

  if (Context.Phase == ModuleUpdatePhase::ImGuiRender) {
    Context.App.ForEachLayer([](Layer *Layer) { Layer->OnImGuiRender(); });
  }
}

void LayerRenderModule::Shutdown(Application &App) { (void)App; }

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
