#include "GlfwEditorLayer.h"

#include <Core/Application.h>
#include <Core/GlfwEditorInputSource.h>
#include <Core/Window.h>
#include <Core/WindowInputPlatform.h>

#include <Renderer/RenderCommand.h>
#include <Session/StartupScene.h>

namespace Axiom {
GlfwEditorLayer::GlfwEditorLayer()
    : Layer("GlfwEditorLayer"), m_Session(m_SessionId) {}

void GlfwEditorLayer::OnAttach() {
  m_Session.EnsureViewportState(m_LocalUserId);
  Window *Window = Application::Get().GetWindow();
  if (Window != nullptr) {
    m_WindowInputPlatform = std::make_unique<WindowInputPlatform>(*Window);
    m_InputSource =
        std::make_unique<GlfwEditorInputSource>(*m_WindowInputPlatform,
                                                m_MoveSpeed);
  }
  LoadStartupScene(m_Session);
}

void GlfwEditorLayer::OnDetach() {
  if (m_WindowInputPlatform != nullptr) {
    m_WindowInputPlatform->SetCursorMode(CursorMode::Normal);
  }
}

void GlfwEditorLayer::OnUpdate() {
  const EditorViewportState *Viewport = m_Session.FindViewport(m_LocalUserId);
  if (m_InputSource != nullptr) {
    m_InputSource->Tick({
        .Session = m_SessionId,
        .User = m_LocalUserId,
        .FrameIndex = Application::Get().GetFrameIndex(),
        .DeltaTimeSeconds = Application::Get().GetDeltaTime(),
        .Viewport = Viewport,
        .CommandSink = &m_Session,
    });
  }
  m_Session.Tick();
  if (m_InputSource != nullptr) {
    m_InputSource->SyncViewport(m_Session.FindViewport(m_LocalUserId));
  }
}

void GlfwEditorLayer::OnRender() {
  const EditorViewportState *Viewport = m_Session.FindViewport(m_LocalUserId);
  if (Viewport == nullptr) {
    return;
  }

  RenderCommand::SetCamera(Viewport->Camera);
  for (const auto &Submission : m_Session.GetState().SceneSubmissions) {
    RenderCommand::Submit(Submission);
  }
}
} // namespace Axiom
