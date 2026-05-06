#include "HeadlessSessionHost.h"

#include <Renderer/VideoEncoderFactory.h>

namespace Axiom {
HeadlessSessionHost::HeadlessSessionHost(const ApplicationArgs &Args,
                                         uint32_t Width, uint32_t Height)
    : Application({.Title = "Axiom Headless",
                   .Width = Width,
                   .Height = Height,
                   .Mode = RuntimeMode::HeadlessEditorSession},
                  Args) {
  m_Layer = new HeadlessSessionLayer();
  PushLayer(m_Layer);
  m_Endpoint = std::make_unique<AxiomSessionEndpoint>(m_Layer->GetSession());
  m_Endpoint->SetVideoEncoder(CreateDefaultVideoEncoder());
  SetViewportFrameOutput(m_Endpoint.get());
}

bool HeadlessSessionHost::LoadStartupSceneIntoSession() {
  return m_Layer->LoadStartupSceneIntoSession();
}

void HeadlessSessionHost::SubmitLocalCommand(const EditorCommand &Command) {
  m_Layer->Submit(Command);
}

void HeadlessSessionHost::SubmitRemoteCommand(const EditorCommand &Command) {
  m_Layer->SubmitToTransport(*m_Endpoint, Command);
}

void HeadlessSessionHost::SubmitRemoteCommand(SessionUserId User,
                                              const EditorCommand &Command) {
  m_Layer->SubmitToTransport(*m_Endpoint, User, Command);
}

void HeadlessSessionHost::SetRemoteViewMode(RendererViewMode ViewMode) {
  SetRendererViewMode(ViewMode);
}

void HeadlessSessionHost::SetActiveRenderUser(SessionUserId User) {
  m_Layer->SetActiveRenderUser(User);
}
} // namespace Axiom
