#include "HeadlessSessionHost.h"

#include <Core/HeadlessRuntimeInstrumentation.h>

#include <algorithm>

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
  m_RenderViews.EnsureLocalView(m_Layer->GetLocalUserId());
  auto TransportModule = std::make_unique<HeadlessSessionTransportModule>(
      m_Layer->GetSession(), [this]() -> std::optional<HeadlessRenderViewState> {
        if (const HeadlessRenderViewState *View = GetActiveRenderView();
            View != nullptr) {
          return *View;
        }
        return std::nullopt;
      });
  m_TransportModule = TransportModule.get();
  GetModuleManager().RegisterModule(std::move(TransportModule));

  auto ScriptingModule = std::make_unique<SessionScriptHostModule>(
      "Headless.SessionScriptHost", m_Layer->GetSession(), SessionId{1},
      m_Layer->GetLocalUserId());
  m_ScriptingModule = ScriptingModule.get();
  GetModuleManager().RegisterModule(std::move(ScriptingModule));
}

bool HeadlessSessionHost::Step() { return Application::Step(); }

std::vector<HeadlessRenderViewState>
HeadlessSessionHost::BuildScheduledRenderPassViews(
    const HeadlessRenderViewRegistry &RenderViews, SessionUserId LocalUserId) {
  std::vector<HeadlessRenderViewState> RenderPassViews =
      RenderViews.BuildRemoteViewSnapshot();
  std::sort(RenderPassViews.begin(), RenderPassViews.end(),
            [](const HeadlessRenderViewState &Left,
               const HeadlessRenderViewState &Right) {
              return Left.User.Value < Right.User.Value;
            });

  if (RenderPassViews.empty()) {
    if (const HeadlessRenderViewState *LocalView =
            RenderViews.FindView(LocalUserId);
        LocalView != nullptr) {
      RenderPassViews.push_back(*LocalView);
    }
  }

  return RenderPassViews;
}

void HeadlessSessionHost::LoadUserScripts(
    const std::filesystem::path &AssemblyPath) {
  GetScriptingModule().GetScriptHost().LoadUserAssembly(AssemblyPath);
  GetScriptingModule().GetScriptHost().StartFileWatcher();
}

void HeadlessSessionHost::ReloadUserScripts() {
  GetScriptingModule().GetScriptHost().ReloadUserAssembly();
}

bool HeadlessSessionHost::LoadStartupSceneIntoSession() {
  return m_Layer->LoadStartupSceneIntoSession();
}

bool HeadlessSessionHost::LoadStartupSceneIntoSession(
    const std::filesystem::path &ContentDir) {
  return m_Layer->LoadStartupSceneIntoSession(ContentDir);
}

void HeadlessSessionHost::SubmitLocalCommand(const EditorCommand &Command) {
  m_Layer->Submit(Command);
}

void HeadlessSessionHost::SubmitRemoteCommand(const EditorCommand &Command) {
  m_Layer->SubmitToTransport(GetTransport(), Command);
}

void HeadlessSessionHost::SubmitRemoteCommand(SessionUserId User,
                                              const EditorCommand &Command) {
  m_Layer->SubmitToTransport(GetTransport(), User, Command);
}

void HeadlessSessionHost::SetTransportVideoEncoder(
    std::unique_ptr<IVideoEncoder> Encoder) {
  m_TransportModule->SetVideoEncoder(std::move(Encoder));
}

void HeadlessSessionHost::SetRemoteViewMode(RendererViewMode ViewMode) {
  m_RenderViews.SetViewMode(m_Layer->GetLocalUserId(), ViewMode);
}

void HeadlessSessionHost::SetRemoteViewMode(SessionUserId User,
                                            RendererViewMode ViewMode) {
  m_RenderViews.SetViewMode(User, ViewMode);
}

void HeadlessSessionHost::SetRemoteShowColliders(bool ShowColliders) {
  m_RenderViews.SetShowColliders(m_Layer->GetLocalUserId(), ShowColliders);
}

void HeadlessSessionHost::SetRemoteShowColliders(SessionUserId User,
                                                 bool ShowColliders) {
  m_RenderViews.SetShowColliders(User, ShowColliders);
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

  m_ActiveRenderPassViews =
      BuildScheduledRenderPassViews(m_RenderViews, m_Layer->GetLocalUserId());
  HeadlessRuntimeInstrumentation::RecordHeadlessTick(
      GetFrameIndex(), m_ActiveRenderPassViews.size(),
      m_RenderViews.GetRemoteViewCount());
  return m_ActiveRenderPassViews.size();
}

void HeadlessSessionHost::PrepareRenderPass(size_t PassIndex) {
  m_CurrentRenderPassIndex = PassIndex;
  if (PassIndex < m_ActiveRenderPassViews.size()) {
    const auto &View = m_ActiveRenderPassViews[PassIndex];
    HeadlessRuntimeInstrumentation::RecordHeadlessRenderPass(
        GetFrameIndex(), PassIndex, View.ClientId, View.User, View.IsLocal);
  }
}

bool HeadlessSessionHost::ShouldRenderImGuiForPass(size_t PassIndex,
                                                   size_t PassCount) const {
  (void)PassIndex;
  (void)PassCount;
  return false;
}
} // namespace Axiom
