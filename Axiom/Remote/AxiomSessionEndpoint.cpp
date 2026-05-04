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

void AxiomSessionEndpoint::OnEditorEvent(const PublishedEditorEvent &Event) {
  for (IAxiomSessionEndpointSubscriber *Subscriber : m_Subscribers) {
    Subscriber->OnAxiomEditorEvent(Event);
  }
}

void AxiomSessionEndpoint::OnViewportFrame(const ViewportFrame &Frame) {
  for (IAxiomSessionEndpointSubscriber *Subscriber : m_Subscribers) {
    Subscriber->OnAxiomViewportFrame(Frame);
  }
}

void AxiomSessionEndpoint::Subscribe(
    IAxiomSessionEndpointSubscriber *Subscriber) {
  if (Subscriber == nullptr) {
    return;
  }

  if (std::find(m_Subscribers.begin(), m_Subscribers.end(), Subscriber) ==
      m_Subscribers.end()) {
    m_Subscribers.push_back(Subscriber);
  }
}

void AxiomSessionEndpoint::Unsubscribe(
    IAxiomSessionEndpointSubscriber *Subscriber) {
  const auto It =
      std::remove(m_Subscribers.begin(), m_Subscribers.end(), Subscriber);
  m_Subscribers.erase(It, m_Subscribers.end());
}
} // namespace Axiom
