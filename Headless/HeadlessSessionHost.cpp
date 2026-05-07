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
  m_Layer->SetSharedRendererAdapter(&m_SharedRendererAdapter);
  m_Layer->SetRenderViewResolver(
      [this]() -> std::optional<HeadlessRenderViewState> {
        if (const HeadlessRenderViewState *View = GetActiveRenderView();
            View != nullptr) {
          return *View;
        }
        return std::nullopt;
      });
  PushLayer(m_Layer);
  m_Endpoint = std::make_unique<AxiomSessionEndpoint>(m_Layer->GetSession());
  m_Endpoint->SetVideoEncoder(CreateDefaultVideoEncoder());
  SetViewportFrameOutput(m_Endpoint.get());
  m_RenderViews.EnsureLocalView(m_Layer->GetLocalUserId());
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
  m_RenderViews.SetViewMode(m_Layer->GetLocalUserId(), ViewMode);
}

void HeadlessSessionHost::SetRemoteViewMode(SessionUserId User,
                                            RendererViewMode ViewMode) {
  m_RenderViews.SetViewMode(User, ViewMode);
}

void HeadlessSessionHost::EnsureRemoteRenderView(const std::string &ClientId,
                                                 SessionUserId User) {
  m_RenderViews.UpsertRemoteView(ClientId, User);
}

void HeadlessSessionHost::RemoveRemoteRenderView(std::string_view ClientId) {
  m_RenderViews.RemoveRemoteView(ClientId);
}

void HeadlessSessionHost::FocusRemoteRenderView(std::string_view ClientId) {
  m_RenderViews.FocusRemoteView(ClientId);
}

void HeadlessSessionHost::FocusLocalRenderView() {
  m_RenderViews.FocusLocalView();
}

const HeadlessRenderViewState *HeadlessSessionHost::GetActiveRenderView() const {
  return m_RenderViews.GetFocusedView();
}

const HeadlessRenderViewState *
HeadlessSessionHost::FindRemoteRenderView(std::string_view ClientId) const {
  return m_RenderViews.FindRemoteView(ClientId);
}

const HeadlessRenderViewState *
HeadlessSessionHost::FindRenderView(SessionUserId User) const {
  return m_RenderViews.FindView(User);
}
} // namespace Axiom
