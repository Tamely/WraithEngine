#include "Remote/AxiomSessionEndpoint.h"

#include "Session/EditorSession.h"

#include <algorithm>

namespace Axiom {
AxiomSessionEndpoint::AxiomSessionEndpoint(EditorSession &Session)
    : m_Session(Session) {
  m_Session.Subscribe(this);
}

AxiomSessionEndpoint::~AxiomSessionEndpoint() { m_Session.Unsubscribe(this); }

void AxiomSessionEndpoint::Submit(const CommandContext &Context,
                                  const EditorCommand &Command) {
  m_Session.Submit(Context, Command);
}

void AxiomSessionEndpoint::Connect(ISessionTransportSubscriber *Subscriber) {
  if (Subscriber == nullptr) {
    return;
  }

  if (std::find(m_Subscribers.begin(), m_Subscribers.end(), Subscriber) ==
      m_Subscribers.end()) {
    m_Subscribers.push_back(Subscriber);
    Subscriber->OnSessionTransportConnected();
  }
}

void AxiomSessionEndpoint::Disconnect(ISessionTransportSubscriber *Subscriber) {
  if (Subscriber == nullptr) {
    return;
  }

  const auto Existing =
      std::find(m_Subscribers.begin(), m_Subscribers.end(), Subscriber);
  if (Existing == m_Subscribers.end()) {
    return;
  }

  Subscriber->OnSessionTransportDisconnected();
  m_Subscribers.erase(Existing);
}

void AxiomSessionEndpoint::OnEditorEvent(const PublishedEditorEvent &Event) {
  for (ISessionTransportSubscriber *Subscriber : m_Subscribers) {
    Subscriber->OnSessionTransportEditorEvent(Event);
  }
}

void AxiomSessionEndpoint::OnViewportFrame(const ViewportFrame &Frame) {
  for (ISessionTransportSubscriber *Subscriber : m_Subscribers) {
    Subscriber->OnSessionTransportViewportFrame(Frame);
  }

  if (m_VideoEncoder != nullptr) {
    m_VideoEncoder->EncodeFrame({
        .FrameIndex = Frame.FrameIndex,
        .Width = Frame.Width,
        .Height = Frame.Height,
        .Format = Frame.Format,
        .Pixels = Frame.Pixels,
    });
  }
}

void AxiomSessionEndpoint::OnEncodedVideoPacket(
    const EncodedVideoPacket &Packet) {
  for (ISessionTransportSubscriber *Subscriber : m_Subscribers) {
    Subscriber->OnSessionTransportEncodedVideoPacket(Packet);
  }
}

void AxiomSessionEndpoint::SetVideoEncoder(std::unique_ptr<IVideoEncoder> Encoder) {
  m_VideoEncoder = std::move(Encoder);
  if (m_VideoEncoder != nullptr) {
    m_VideoEncoder->SetOutput(this);
  }
}
} // namespace Axiom
