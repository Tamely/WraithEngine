#include "GlfwEditorLayer.h"

#include <Core/Application.h>
#include <Core/Window.h>
#include <Session/StartupScene.h>

namespace Axiom {
GlfwEditorLayer::GlfwEditorLayer()
    : Layer("GlfwEditorLayer"), m_Session(m_SessionId) {}

void GlfwEditorLayer::OnAttach() {
  m_Session.EnsureViewportState(m_LocalUserId);
  Window *Window = Application::Get().GetWindow();
  if (Window != nullptr) {
    m_InputModule.Initialize(*Window, m_MoveSpeed);
  }
  LoadStartupScene(m_Session);
}

void GlfwEditorLayer::OnDetach() { m_InputModule.Shutdown(); }

void GlfwEditorLayer::OnUpdate() {
  m_InputModule.Tick(m_Session, m_SessionId, m_LocalUserId);
  m_Session.Tick();
  m_SelectionModule.Tick(m_Session, m_SessionId, m_LocalUserId,
                         m_InputModule.GetInputPlatform(),
                         Application::Get().GetWindow(), m_LastLeftMouseDown);
}

void GlfwEditorLayer::OnRender() {
  m_RenderModule.Render(m_Session, m_LocalUserId, m_RendererAdapter);
}
} // namespace Axiom
