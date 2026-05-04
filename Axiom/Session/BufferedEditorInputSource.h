#pragma once

#include "Session/EditorInputSource.h"

#include <deque>

namespace Axiom {
class BufferedEditorInputSource final : public IEditorInputSource {
public:
  void Enqueue(EditorCommand Command);

  void Tick(const EditorInputFrame &Frame) override;
  void SyncViewport(const EditorViewportState *Viewport) override;

private:
  std::deque<EditorCommand> m_QueuedCommands;
};
} // namespace Axiom
