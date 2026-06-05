#pragma once

#include <Core/InputPlatform.h>
#include <Session/EditorInputSource.h>
#include <Session/EditorSceneRendererAdapter.h>
#include <Session/EditorSession.h>

#include <memory>

namespace Axiom {
class Window;

class EditorViewportInputModule {
public:
  void Initialize(Window &Window, float MoveSpeed);
  void Shutdown();
  void Tick(EditorSession &Session, SessionId SessionHandle,
            SessionUserId LocalUserId);

  IInputPlatform *GetInputPlatform() const { return m_WindowInputPlatform.get(); }

private:
  std::unique_ptr<IInputPlatform> m_WindowInputPlatform;
  std::unique_ptr<IEditorInputSource> m_InputSource;
};

class EditorViewportSelectionModule {
public:
  void Tick(EditorSession &Session, SessionId SessionHandle,
            SessionUserId LocalUserId, IInputPlatform *InputPlatform,
            const Window *Window, bool &LastLeftMouseDown);
};

class EditorSceneRenderModule {
public:
  void Render(EditorSession &Session, SessionUserId LocalUserId,
              EditorSceneRendererAdapter &RendererAdapter);
};
} // namespace Axiom
