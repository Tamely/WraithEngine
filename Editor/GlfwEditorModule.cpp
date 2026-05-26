#include "GlfwEditorModule.h"

#include <Core/Application.h>
#include <Core/Window.h>
#include <Session/StartupScene.h>

namespace Axiom {
GlfwEditorModule::GlfwEditorModule() : m_Session(m_SessionId) {}

std::string_view GlfwEditorModule::GetName() const {
  return "Editor.GlfwEditor";
}

bool GlfwEditorModule::Initialize(Application &App) {
  m_Session.EnsureViewportState(m_LocalUserId);
  Window *Window = App.GetWindow();
  if (Window != nullptr) {
    m_InputModule.Initialize(*Window, m_MoveSpeed);
  }
  return LoadStartupScene(m_Session);
}

void GlfwEditorModule::Update(const ModuleUpdateContext &Context) {
  switch (Context.Phase) {
  case ModuleUpdatePhase::FrameStart:
    m_InputModule.Tick(m_Session, m_SessionId, m_LocalUserId);
    m_Session.Tick();
    m_SelectionModule.Tick(m_Session, m_SessionId, m_LocalUserId,
                           m_InputModule.GetInputPlatform(),
                           Context.App.GetWindow(), m_LastLeftMouseDown);
    break;
  case ModuleUpdatePhase::Render:
    m_RenderModule.Render(m_Session, m_LocalUserId, m_RendererAdapter);
    break;
  case ModuleUpdatePhase::RenderBegin:
  case ModuleUpdatePhase::ImGuiRender:
  case ModuleUpdatePhase::RenderEnd:
    break;
  }
}

void GlfwEditorModule::Shutdown(Application &App) {
  (void)App;
  m_InputModule.Shutdown();
}
} // namespace Axiom
