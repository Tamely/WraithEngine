#pragma once

#include "Renderer/ViewportFrameOutput.h"
#include "Session/EditorCommand.h"
#include "Session/EditorEvent.h"

#include <vector>

namespace Axiom {
class EditorSession;

class IAxiomSessionEndpointSubscriber {
public:
  virtual ~IAxiomSessionEndpointSubscriber() = default;

  virtual void OnAxiomEditorEvent(const PublishedEditorEvent &Event) = 0;
  virtual void OnAxiomViewportFrame(const ViewportFrame &Frame) = 0;
};

class AxiomSessionEndpoint final : public IEditorCommandSink,
                                   public IEditorEventSubscriber,
                                   public IViewportFrameOutput {
public:
  explicit AxiomSessionEndpoint(EditorSession &Session);
  ~AxiomSessionEndpoint() override;

  void Submit(const CommandContext &Context,
              const EditorCommand &Command) override;
  void OnEditorEvent(const PublishedEditorEvent &Event) override;
  void OnViewportFrame(const ViewportFrame &Frame) override;

  void Subscribe(IAxiomSessionEndpointSubscriber *Subscriber);
  void Unsubscribe(IAxiomSessionEndpointSubscriber *Subscriber);

private:
  EditorSession &m_Session;
  std::vector<IAxiomSessionEndpointSubscriber *> m_Subscribers;
};
} // namespace Axiom
