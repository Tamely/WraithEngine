#pragma once

#include "Remote/SessionTransport.h"
#include "Renderer/VideoEncoding.h"
#include "Renderer/ViewportFrameOutput.h"
#include "Session/EditorEvent.h"

#include <memory>
#include <vector>

namespace Axiom {
class EditorSession;

class AxiomSessionEndpoint final : public ISessionTransport,
                                   public IEditorEventSubscriber,
                                   public IViewportFrameOutput,
                                   public IEncodedVideoPacketOutput {
public:
  explicit AxiomSessionEndpoint(EditorSession &Session);
  ~AxiomSessionEndpoint() override;

  void Submit(const CommandContext &Context,
              const EditorCommand &Command) override;
  void Connect(ISessionTransportSubscriber *Subscriber) override;
  void Disconnect(ISessionTransportSubscriber *Subscriber) override;
  void OnEditorEvent(const PublishedEditorEvent &Event) override;
  void OnViewportFrame(const ViewportFrame &Frame) override;
  void OnEncodedVideoPacket(const EncodedVideoPacket &Packet) override;
  void SetVideoEncoder(std::unique_ptr<IVideoEncoder> Encoder);

private:
  EditorSession &m_Session;
  std::vector<ISessionTransportSubscriber *> m_Subscribers;
  std::unique_ptr<IVideoEncoder> m_VideoEncoder;
};
} // namespace Axiom
