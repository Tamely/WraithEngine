#include "Session/BufferedEditorInputSource.h"

#include <utility>

namespace Axiom {
void BufferedEditorInputSource::Enqueue(EditorCommand Command) {
  m_QueuedCommands.push_back(std::move(Command));
}

void BufferedEditorInputSource::Tick(const EditorInputFrame &Frame) {
  if (Frame.CommandSink == nullptr) {
    return;
  }

  CommandContext Context{
      .Session = Frame.Session,
      .User = Frame.User,
      .FrameIndex = Frame.FrameIndex,
      .DeltaTimeSeconds = Frame.DeltaTimeSeconds,
  };

  while (!m_QueuedCommands.empty()) {
    Frame.CommandSink->Submit(Context, m_QueuedCommands.front());
    m_QueuedCommands.pop_front();
  }
}

void BufferedEditorInputSource::SyncViewport(
    const EditorViewportState *Viewport) {
  (void)Viewport;
}
} // namespace Axiom
