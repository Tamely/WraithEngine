#include "GlfwEditorLayer.h"

#include <Core/Application.h>
#include <Core/GlfwEditorInputSource.h>
#include <Core/Window.h>
#include <Core/WindowInputPlatform.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <Renderer/RenderCommand.h>
#include <Session/MeshPicking.h>
#include <Session/StartupScene.h>

namespace Axiom {
GlfwEditorLayer::GlfwEditorLayer()
    : Layer("GlfwEditorLayer"), m_Session(m_SessionId) {}

void GlfwEditorLayer::OnAttach() {
  m_Session.EnsureViewportState(m_LocalUserId);
  Window *Window = Application::Get().GetWindow();
  if (Window != nullptr) {
    m_WindowInputPlatform = std::make_unique<WindowInputPlatform>(*Window);
    m_InputSource =
        std::make_unique<GlfwEditorInputSource>(*m_WindowInputPlatform,
                                                m_MoveSpeed);
  }
  LoadStartupScene(m_Session);
}

void GlfwEditorLayer::OnDetach() {
  if (m_WindowInputPlatform != nullptr) {
    m_WindowInputPlatform->SetCursorMode(CursorMode::Normal);
  }
}

void GlfwEditorLayer::OnUpdate() {
  const EditorViewportState *Viewport = m_Session.FindViewport(m_LocalUserId);
  if (m_InputSource != nullptr) {
    m_InputSource->Tick({
        .Session = m_SessionId,
        .User = m_LocalUserId,
        .FrameIndex = Application::Get().GetFrameIndex(),
        .DeltaTimeSeconds = Application::Get().GetDeltaTime(),
        .Viewport = Viewport,
        .CommandSink = &m_Session,
    });
  }
  m_Session.Tick();

  // Left-click mesh picking — detect rising edge to select on click, not hold.
  {
    const bool IsLeftDown = m_WindowInputPlatform != nullptr &&
        m_WindowInputPlatform->IsMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);
    const bool ClickedNow = IsLeftDown && !m_LastLeftMouseDown;
    m_LastLeftMouseDown = IsLeftDown;

    if (ClickedNow && Viewport != nullptr && !Viewport->IsLooking) {
      const Window *Win = Application::Get().GetWindow();
      if (Win != nullptr) {
        const glm::dvec2 CursorPos = m_WindowInputPlatform->GetCursorPosition();
        const std::string HitId = HitTestMeshes(
            Viewport->Camera, Win->GetWidth(), Win->GetHeight(),
            glm::vec2(CursorPos), m_Session.GetState().Scene.MeshInstances);
        if (!HitId.empty()) {
          const CommandContext Ctx{
              .Session = m_SessionId,
              .User = m_LocalUserId,
              .FrameIndex = Application::Get().GetFrameIndex(),
              .DeltaTimeSeconds = Application::Get().GetDeltaTime(),
          };
          m_Session.Submit(Ctx, EditorCommand{SelectObjectCommand{.ObjectId = HitId}});
        }
      }
    }
  }

  if (m_InputSource != nullptr) {
    m_InputSource->SyncViewport(m_Session.FindViewport(m_LocalUserId));
  }
}

void GlfwEditorLayer::OnRender() {
  const EditorViewportState *Viewport = m_Session.FindViewport(m_LocalUserId);
  if (Viewport == nullptr) {
    return;
  }

  RenderCommand::SetCamera(Viewport->Camera);
  for (const auto &Submission : m_RendererAdapter.BuildRenderSubmissions(m_Session)) {
    RenderCommand::Submit(Submission);
  }

  const EditorObjectDetails *Selected =
      m_Session.FindSelectedObjectDetails(m_LocalUserId);
  if (Selected != nullptr && Selected->SupportsTransform &&
      Selected->Transform.has_value()) {
    RenderCommand::SetGizmoOverlay(
        {.WorldPosition = Selected->Transform->Location, .Scale = 0.5f});
  }
}
} // namespace Axiom
