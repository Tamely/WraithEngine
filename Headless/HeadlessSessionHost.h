#pragma once

#include <Core/Application.h>
#include <Remote/AxiomSessionEndpoint.h>

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
  void SetRemoteViewMode(RendererViewMode ViewMode);
  ISessionTransport &GetTransport() { return *m_Endpoint; }
  HeadlessSessionLayer &GetHeadlessLayer() { return *m_Layer; }

private:
  HeadlessSessionLayer *m_Layer{nullptr};
  std::unique_ptr<AxiomSessionEndpoint> m_Endpoint;
};
} // namespace Axiom
