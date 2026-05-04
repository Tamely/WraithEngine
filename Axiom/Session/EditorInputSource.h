#pragma once

#include "Session/EditorCommand.h"

#include <cstdint>

namespace Axiom {
struct EditorViewportState;

struct EditorInputFrame {
  SessionId Session;
  SessionUserId User;
  uint64_t FrameIndex{0};
  float DeltaTimeSeconds{0.0f};
  const EditorViewportState *Viewport{nullptr};
  IEditorCommandSink *CommandSink{nullptr};
};

class IEditorInputSource {
public:
  virtual ~IEditorInputSource() = default;

  virtual void Tick(const EditorInputFrame &Frame) = 0;
  virtual void SyncViewport(const EditorViewportState *Viewport) = 0;
};
} // namespace Axiom
