#pragma once

#include <Core/Application.h>
#include <Remote/AxiomSessionEndpoint.h>
#include <Renderer/VideoEncoding.h>
#include <Scripting/ScriptHost.h>
#include <Session/EditorSceneRendererAdapter.h>

#include "HeadlessRenderView.h"
#include "HeadlessSessionLayer.h"
#include "HeadlessViewportFrameBridge.h"

#include <memory>
#include <vector>

namespace Axiom {
class HeadlessSessionHost final : public Application {
public:
  HeadlessSessionHost(const ApplicationArgs &Args, uint32_t Width,
                      uint32_t Height);
  bool Step();

  bool LoadStartupSceneIntoSession();
  void SubmitLocalCommand(const EditorCommand &Command);
  void SubmitRemoteCommand(const EditorCommand &Command);
  void SubmitRemoteCommand(SessionUserId User, const EditorCommand &Command);
  void SetTransportVideoEncoder(std::unique_ptr<IVideoEncoder> Encoder);
  void SetRemoteViewMode(RendererViewMode ViewMode);
  void SetRemoteViewMode(SessionUserId User, RendererViewMode ViewMode);
  void EnsureRemoteRenderView(const std::string &ClientId, SessionUserId User);
  void RemoveRemoteRenderView(std::string_view ClientId);
  void FocusRemoteRenderView(std::string_view ClientId);
  void FocusLocalRenderView();
  const HeadlessRenderViewState *GetActiveRenderView() const;
  const HeadlessRenderViewState *FindRemoteRenderView(
      std::string_view ClientId) const;
  const HeadlessRenderViewState *FindRenderView(SessionUserId User) const;
  ISessionTransport &GetTransport() { return *m_Endpoint; }
  HeadlessSessionLayer &GetHeadlessLayer() { return *m_Layer; }
  const HeadlessRenderViewRegistry &GetRenderViews() const {
    return m_RenderViews;
  }

private:
  size_t BeginRenderPasses() override;
  void PrepareRenderPass(size_t PassIndex) override;
  bool ShouldRenderImGuiForPass(size_t PassIndex,
                                size_t PassCount) const override;

  HeadlessSessionLayer *m_Layer{nullptr};
  std::unique_ptr<AxiomSessionEndpoint> m_Endpoint;
  std::unique_ptr<HeadlessViewportFrameBridge> m_FrameBridge;
  EditorSceneRendererAdapter m_SharedRendererAdapter;
  HeadlessRenderViewRegistry m_RenderViews;
  std::vector<HeadlessRenderViewState> m_ActiveRenderPassViews;
  size_t m_CurrentRenderPassIndex{0};
  ScriptHost m_ScriptHost;
};
} // namespace Axiom
