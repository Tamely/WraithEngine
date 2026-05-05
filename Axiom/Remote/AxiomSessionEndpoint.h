#pragma once

#include "Remote/SessionTransport.h"
#include "Renderer/ViewportFrameOutput.h"
#include "Session/EditorEvent.h"

#include <vector>

namespace Axiom {
class EditorSession;

class AxiomSessionEndpoint final : public ISessionTransport,
                                   public IEditorEventSubscriber,
                                   public IViewportFrameOutput {
public:
  explicit AxiomSessionEndpoint(EditorSession &Session);
  ~AxiomSessionEndpoint() override;

  void Submit(const CommandContext &Context,
              const EditorCommand &Command) override;
  void Connect(ISessionTransportSubscriber *Subscriber) override;
  void Disconnect(ISessionTransportSubscriber *Subscriber) override;
  void OnEditorEvent(const PublishedEditorEvent &Event) override;
  void OnViewportFrame(const ViewportFrame &Frame) override;

private:
  EditorSession &m_Session;
  std::vector<ISessionTransportSubscriber *> m_Subscribers;
};
} // namespace Axiom
