#include "HeadlessSessionLayer.h"

#include <Core/Application.h>

#include <Renderer/RenderCommand.h>
#include <Session/StartupScene.h>

namespace Axiom {
HeadlessSessionLayer::HeadlessSessionLayer()
    : Layer("HeadlessSessionLayer"), m_Session(m_SessionId) {}

void HeadlessSessionLayer::OnAttach() {
  m_Session.EnsureViewportState(m_LocalUserId);
  m_ActiveRenderUserId = m_LocalUserId;
}

void HeadlessSessionLayer::OnUpdate() { m_Session.Tick(); }

void HeadlessSessionLayer::OnRender() {
  const EditorViewportState *Viewport = m_Session.FindViewport(m_ActiveRenderUserId);
  if (Viewport == nullptr && m_ActiveRenderUserId.Value != m_LocalUserId.Value) {
    Viewport = m_Session.FindViewport(m_LocalUserId);
  }
  if (Viewport == nullptr) {
    return;
  }

  RenderCommand::SetCamera(Viewport->Camera);
  for (const auto &Submission : m_Session.GetState().SceneSubmissions) {
    RenderCommand::Submit(Submission);
  }
}

bool HeadlessSessionLayer::LoadStartupSceneIntoSession() {
  return LoadStartupScene(m_Session);
}

void HeadlessSessionLayer::Submit(const EditorCommand &Command) {
  m_Session.Submit(MakeContext(), Command);
}

void HeadlessSessionLayer::SubmitToTransport(ISessionTransport &Transport,
                                             const EditorCommand &Command) {
  Transport.Submit(MakeContext(), Command);
}

void HeadlessSessionLayer::SubmitToTransport(ISessionTransport &Transport,
                                             SessionUserId User,
                                             const EditorCommand &Command) {
  Transport.Submit(MakeContext(User), Command);
}

CommandContext HeadlessSessionLayer::MakeContext() const {
  return MakeContext(m_LocalUserId);
}

CommandContext HeadlessSessionLayer::MakeContext(SessionUserId User) const {
  return {
      .Session = m_SessionId,
      .User = User,
      .FrameIndex = Application::Get().GetFrameIndex(),
      .DeltaTimeSeconds = Application::Get().GetDeltaTime(),
  };
}
} // namespace Axiom
