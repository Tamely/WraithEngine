#pragma once

#include "Session/EditorCommand.h"
#include "Session/EditorEvent.h"

#include <deque>
#include <functional>
#include <vector>

namespace Axiom {
class EditorMessageBus {
public:
  CommandId EnqueueCommand(const CommandContext &Context,
                           const EditorCommand &Command);
  void DispatchQueuedCommands(
      const std::function<void(const QueuedEditorCommand &)> &Handler);

  EventId PublishEvent(const EditorEvent &Event);

  void Subscribe(IEditorEventSubscriber *Subscriber);
  void Unsubscribe(IEditorEventSubscriber *Subscriber);

private:
  CommandId m_NextCommandId{1};
  EventId m_NextEventId{1};
  std::deque<QueuedEditorCommand> m_CommandQueue;
  std::vector<IEditorEventSubscriber *> m_Subscribers;
};
} // namespace Axiom
