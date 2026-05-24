#include "EditorFeatureModules.h"

#include <Core/Application.h>
#include <Core/GlfwEditorInputSource.h>
#include <Core/Window.h>
#include <Core/WindowInputPlatform.h>
#include <Renderer/RenderCommand.h>
#include <Session/MeshPicking.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace Axiom {
void EditorViewportInputModule::Initialize(Window &Window, float MoveSpeed) {
  m_WindowInputPlatform = std::make_unique<WindowInputPlatform>(Window);
  m_InputSource =
      std::make_unique<GlfwEditorInputSource>(*m_WindowInputPlatform, MoveSpeed);
}

void EditorViewportInputModule::Shutdown() {
  if (m_WindowInputPlatform != nullptr) {
    m_WindowInputPlatform->SetCursorMode(CursorMode::Normal);
  }
  m_InputSource.reset();
  m_WindowInputPlatform.reset();
}

void EditorViewportInputModule::Tick(EditorSession &Session,
                                     SessionId SessionHandle,
                                     SessionUserId LocalUserId) {
  const EditorViewportState *Viewport = Session.FindViewport(LocalUserId);
  if (m_InputSource != nullptr) {
    m_InputSource->Tick({
        .Session = SessionHandle,
        .User = LocalUserId,
        .FrameIndex = Application::Get().GetFrameIndex(),
        .DeltaTimeSeconds = Application::Get().GetDeltaTime(),
        .Viewport = Viewport,
        .CommandSink = &Session,
    });
  }

  if (m_InputSource != nullptr) {
    m_InputSource->SyncViewport(Session.FindViewport(LocalUserId));
  }
}

void EditorViewportSelectionModule::Tick(EditorSession &Session,
                                         SessionId SessionHandle,
                                         SessionUserId LocalUserId,
                                         IInputPlatform *InputPlatform,
                                         const Window *Window,
                                         bool &LastLeftMouseDown) {
  const EditorViewportState *Viewport = Session.FindViewport(LocalUserId);
  const bool IsLeftDown = InputPlatform != nullptr &&
      InputPlatform->IsMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);
  const bool ClickedNow = IsLeftDown && !LastLeftMouseDown;
  LastLeftMouseDown = IsLeftDown;

  if (!ClickedNow || Viewport == nullptr || Viewport->IsLooking ||
      InputPlatform == nullptr || Window == nullptr) {
    return;
  }

  const glm::dvec2 CursorPos = InputPlatform->GetCursorPosition();
  const std::string HitId =
      HitTestMeshes(Viewport->Camera, Window->GetWidth(), Window->GetHeight(),
                    glm::vec2(CursorPos), Session.GetState().Scene.MeshInstances);
  if (HitId.empty()) {
    return;
  }

  const CommandContext Context{
      .Session = SessionHandle,
      .User = LocalUserId,
      .FrameIndex = Application::Get().GetFrameIndex(),
      .DeltaTimeSeconds = Application::Get().GetDeltaTime(),
  };
  Session.Submit(Context, EditorCommand{SelectObjectCommand{.ObjectId = HitId}});
}

void EditorSceneRenderModule::Render(EditorSession &Session,
                                     SessionUserId LocalUserId,
                                     EditorSceneRendererAdapter &RendererAdapter) {
  const EditorViewportState *Viewport = Session.FindViewport(LocalUserId);
  if (Viewport == nullptr) {
    return;
  }

  RenderCommand::SetCamera(Viewport->Camera);
  for (const auto &Submission : RendererAdapter.BuildRenderSubmissions(Session)) {
    RenderCommand::Submit(Submission);
  }

  const EditorObjectDetails *Selected =
      Session.FindSelectedObjectDetails(LocalUserId);
  if (Selected != nullptr && Selected->SupportsTransform &&
      Selected->Transform.has_value()) {
    RenderCommand::SetGizmoOverlay(
        {.WorldPosition = Selected->Transform->Location, .Scale = 0.5f});
  }
}
} // namespace Axiom
