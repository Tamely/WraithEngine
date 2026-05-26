#pragma once

#include <Core/IModule.h>

#include "EditorFeatureModules.h"

#include <Session/EditorSceneRendererAdapter.h>
#include <Session/EditorSession.h>

namespace Axiom {
class GlfwEditorModule final : public IModule {
public:
  GlfwEditorModule();

  [[nodiscard]] std::string_view GetName() const override;
  bool Initialize(Application &App) override;
  void Update(const ModuleUpdateContext &Context) override;
  void Shutdown(Application &App) override;

private:
  SessionId m_SessionId{1};
  SessionUserId m_LocalUserId{1};
  EditorSession m_Session;
  EditorSceneRendererAdapter m_RendererAdapter;
  EditorViewportInputModule m_InputModule;
  EditorViewportSelectionModule m_SelectionModule;
  EditorSceneRenderModule m_RenderModule;
  float m_MoveSpeed{3.5f};
  bool m_LastLeftMouseDown{false};
};
} // namespace Axiom
