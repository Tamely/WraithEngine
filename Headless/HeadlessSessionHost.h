#pragma once

#include <Core/Application.h>
#include <Remote/AxiomSessionEndpoint.h>
#include <Session/EditorSceneRendererAdapter.h>

#include "HeadlessRenderView.h"
#include "HeadlessSessionLayer.h"

#include <memory>

namespace Axiom {
class HeadlessSessionHost final : public Application {
public:
  HeadlessSessionHost(const ApplicationArgs &Args, uint32_t Width,
                      uint32_t Height);

  bool LoadStartupSceneIntoSession();
  void SubmitLocalCommand(const EditorCommand &Command);
  void SubmitRemoteCommand(const EditorCommand &Command);
  void SubmitRemoteCommand(SessionUserId User, const EditorCommand &Command);
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
  HeadlessSessionLayer *m_Layer{nullptr};
  std::unique_ptr<AxiomSessionEndpoint> m_Endpoint;
  EditorSceneRendererAdapter m_SharedRendererAdapter;
  HeadlessRenderViewRegistry m_RenderViews;
};
} // namespace Axiom
