#pragma once

#include "Core/InputPlatform.h"
#include "Session/EditorInputSource.h"

namespace Axiom {
class GlfwEditorInputSource final : public IEditorInputSource {
public:
  GlfwEditorInputSource(IInputPlatform &Platform, float MoveSpeed = 3.5f);

  void Tick(const EditorInputFrame &Frame) override;
  void SyncViewport(const EditorViewportState *Viewport) override;

private:
  IInputPlatform &m_Platform;
  bool m_LastLookInputState{false};
  float m_MoveSpeed{3.5f};
};
} // namespace Axiom
