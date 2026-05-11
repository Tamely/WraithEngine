#include "HeadlessSessionHost.h"

#include <algorithm>

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
  m_RenderViews.EnsureLocalView(m_Layer->GetLocalUserId());
  m_FrameBridge = std::make_unique<HeadlessViewportFrameBridge>(
      *m_Endpoint, [this]() -> std::optional<HeadlessRenderViewState> {
        if (const HeadlessRenderViewState *View = GetActiveRenderView();
            View != nullptr) {
          return *View;
        }
        return std::nullopt;
      });
  SetViewportFrameOutput(m_FrameBridge.get());
  m_ScriptHost.Initialize(
      AXIOM_CORAL_MANAGED_DIR,
      AXIOM_SCRIPTING_TRUST_RESTRICTED ? ScriptTrustProfile::Restricted
                                       : ScriptTrustProfile::Trusted);
  m_ScriptHost.LoadEngineAssembly(AXIOM_MANAGED_DIR);
  m_ScriptHost.RegisterInternalCalls(m_Layer->GetSession(),
                                     SessionId{1},
                                     m_Layer->GetLocalUserId());
  m_Layer->GetSession().Subscribe(&m_ScriptHost);
  m_Layer->SetScriptHost(&m_ScriptHost);
}

bool HeadlessSessionHost::Step() { return Application::Step(); }

void HeadlessSessionHost::LoadUserScripts(
    const std::filesystem::path &AssemblyPath) {
  m_ScriptHost.LoadUserAssembly(AssemblyPath);
  m_ScriptHost.StartFileWatcher();
}

void HeadlessSessionHost::ReloadUserScripts() {
  m_ScriptHost.ReloadUserAssembly();
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

void HeadlessSessionHost::SetTransportVideoEncoder(
    std::unique_ptr<IVideoEncoder> Encoder) {
  m_Endpoint->SetVideoEncoder(std::move(Encoder));
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
  if (m_CurrentRenderPassIndex < m_ActiveRenderPassViews.size()) {
    return &m_ActiveRenderPassViews[m_CurrentRenderPassIndex];
  }
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

size_t HeadlessSessionHost::BeginRenderPasses() {
  m_ActiveRenderPassViews.clear();
  m_CurrentRenderPassIndex = 0;

  m_ActiveRenderPassViews = m_RenderViews.BuildRemoteViewSnapshot();
  std::sort(m_ActiveRenderPassViews.begin(), m_ActiveRenderPassViews.end(),
            [](const HeadlessRenderViewState &Left,
               const HeadlessRenderViewState &Right) {
              return Left.User.Value < Right.User.Value;
            });

  if (m_ActiveRenderPassViews.empty()) {
    if (const HeadlessRenderViewState *LocalView = m_RenderViews.FindView(
            m_Layer->GetLocalUserId());
        LocalView != nullptr) {
      m_ActiveRenderPassViews.push_back(*LocalView);
    }
  }

  return m_ActiveRenderPassViews.size();
}

void HeadlessSessionHost::PrepareRenderPass(size_t PassIndex) {
  m_CurrentRenderPassIndex = PassIndex;
}

bool HeadlessSessionHost::ShouldRenderImGuiForPass(size_t PassIndex,
                                                   size_t PassCount) const {
  (void)PassIndex;
  (void)PassCount;
  return false;
}
} // namespace Axiom
