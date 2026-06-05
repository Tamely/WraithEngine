#include "Session/EditorMessageBus.h"

#include <algorithm>

namespace Axiom {
CommandId EditorMessageBus::EnqueueCommand(const CommandContext &Context,
                                           const EditorCommand &Command) {
  const CommandId Id = m_NextCommandId;
  ++m_NextCommandId.Value;
  m_CommandQueue.push_back({.Id = Id, .Context = Context, .Command = Command});
  return Id;
}

void EditorMessageBus::DispatchQueuedCommands(
    const std::function<void(const QueuedEditorCommand &)> &Handler) {
  while (!m_CommandQueue.empty()) {
    const QueuedEditorCommand Command = m_CommandQueue.front();
    m_CommandQueue.pop_front();
    Handler(Command);
  }
}

EventId EditorMessageBus::PublishEvent(const EditorEvent &Event) {
  const EventId Id = m_NextEventId;
  ++m_NextEventId.Value;
  const PublishedEditorEvent Published{.Id = Id, .Event = Event};
  for (IEditorEventSubscriber *Subscriber : m_Subscribers) {
    if (Subscriber != nullptr) {
      Subscriber->OnEditorEvent(Published);
    }
  }
  return Id;
}

void EditorMessageBus::Subscribe(IEditorEventSubscriber *Subscriber) {
  if (Subscriber == nullptr) {
    return;
  }

  const auto It =
      std::find(m_Subscribers.begin(), m_Subscribers.end(), Subscriber);
  if (It == m_Subscribers.end()) {
    m_Subscribers.push_back(Subscriber);
  }
}

void EditorMessageBus::Unsubscribe(IEditorEventSubscriber *Subscriber) {
  const auto It =
      std::remove(m_Subscribers.begin(), m_Subscribers.end(), Subscriber);
  m_Subscribers.erase(It, m_Subscribers.end());
}
} // namespace Axiom
