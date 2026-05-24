#include "HeadlessSessionLayer.h"

#include "HeadlessRenderView.h"

#include <Core/Application.h>

#include <Renderer/Camera.h>
#include <Renderer/RenderCommand.h>
#include <Session/StartupScene.h>

#include <algorithm>

namespace Axiom {
namespace {
} // namespace

HeadlessSessionLayer::HeadlessSessionLayer()
    : Layer("HeadlessSessionLayer"),
      m_Session(m_SessionId),
      m_OverlayModule(m_Session) {}

void HeadlessSessionLayer::OnAttach() {
  m_Session.EnsureViewportState(m_LocalUserId);
  m_OverlayModule.Initialize();
}

void HeadlessSessionLayer::OnUpdate() {
  m_Session.Tick(Application::Get().GetDeltaTime());
}

void HeadlessSessionLayer::OnRender() {
  HeadlessRenderViewState RenderView{
      .ClientId = "",
      .User = m_LocalUserId,
      .ViewMode = RendererViewMode::Lit,
      .IsLocal = true,
  };
  if (m_RenderViewResolver) {
    if (const auto ResolvedView = m_RenderViewResolver();
        ResolvedView.has_value()) {
      RenderView = *ResolvedView;
    }
  }

  SessionUserId RenderUser = RenderView.User;
  const EditorViewportState *Viewport = m_Session.FindViewport(RenderUser);
  if (Viewport == nullptr && RenderUser.Value != m_LocalUserId.Value) {
    RenderUser = m_LocalUserId;
    Viewport = m_Session.FindViewport(RenderUser);
  }
  if (Viewport == nullptr) {
    return;
  }

  if (m_RendererAdapter == nullptr) {
    return;
  }

  Application::Get().SetRendererViewMode(RenderView.ViewMode);
  Application::Get().SetViewportFrameUser(RenderUser);
  RenderCommand::SetCamera(Viewport->Camera);

  // Pick the first visible Light that has LightProperties configured.
  for (const auto &[Id, Details] : m_Session.GetState().Scene.ObjectDetailsById) {
    if (Details.Kind == EditorSceneItemKind::Light && Details.Visible &&
        Details.Light.has_value()) {
      // Derive direction from the light's world-space position so that moving
      // the object in the editor has an immediate effect on the sun direction.
      glm::vec3 Dir = Details.Light->Direction;
      const EditorTransformDetails *EffTransform =
          Details.WorldTransform.has_value()  ? &*Details.WorldTransform
          : Details.Transform.has_value()     ? &*Details.Transform
                                              : nullptr;
      if (EffTransform != nullptr &&
          glm::length(EffTransform->Location) > 0.001f) {
        Dir = EffTransform->Location;
      }
      RenderCommand::SetSun({
          .Color = Details.Light->Color,
          .Intensity = Details.Light->Intensity,
          .Direction = Dir,
      });
      break;
    }
  }

  RenderCommand::SetSkyboxColors(m_Session.GetState().Scene.WorldSettings.SkyboxColorTop,
                                 m_Session.GetState().Scene.WorldSettings.SkyboxColorBottom);
  RenderCommand::SetSkyboxHDR(m_Session.GetState().Scene.WorldSettings.SkyboxHDRData);

  for (const auto &Submission : m_RendererAdapter->BuildRenderSubmissions(m_Session)) {
    RenderCommand::Submit(Submission);
  }
  for (const auto &Billboard : m_OverlayModule.BuildLightBillboards()) {
    RenderCommand::SubmitLightBillboard(Billboard);
  }
  if (RenderView.ShowColliders) {
    for (const auto &Submission : m_OverlayModule.BuildColliderOverlaySubmissions()) {
      RenderCommand::Submit(Submission);
    }
  }
  for (const auto &Submission :
       m_OverlayModule.BuildPresenceOverlaySubmissions(RenderUser)) {
    RenderCommand::Submit(Submission);
  }

  const EditorObjectDetails *Selected =
      m_Session.FindSelectedObjectDetails(RenderUser);
  const auto *EffTransform = Selected != nullptr
      ? (Selected->WorldTransform.has_value() ? &*Selected->WorldTransform
                                              : (Selected->Transform.has_value()
                                                     ? &*Selected->Transform
                                                     : nullptr))
      : nullptr;
  if (EffTransform != nullptr && Selected->SupportsTransform) {
    RenderCommand::SetGizmoOverlay({
        .WorldPosition = EffTransform->Location,
        .Scale = 0.5f,
        .HoveredAxis = m_OverlayModule.GetGizmoHoveredAxis(RenderUser),
        .Mode = m_OverlayModule.GetGizmoMode(RenderUser),
    });
  }
}

bool HeadlessSessionLayer::LoadStartupSceneIntoSession() {
  return LoadStartupSceneIntoSession(std::filesystem::path(AXIOM_CONTENT_DIR));
}

bool HeadlessSessionLayer::LoadStartupSceneIntoSession(
    const std::filesystem::path &ContentDir) {
#ifndef AXIOM_CONTENT_DIR
#define AXIOM_CONTENT_DIR "Content"
#endif
  m_Session.SetContentDir(ContentDir);
  m_Session.SetEngineContentDir(std::filesystem::path(AXIOM_CONTENT_DIR) / "Engine");
  return LoadStartupScene(m_Session);
}

void HeadlessSessionLayer::Submit(const EditorCommand &Command) {
  m_Session.Submit(MakeContext(), Command);
}

void HeadlessSessionLayer::SubmitToTransport(ISessionTransport &Transport,
                                             const EditorCommand &Command) {
  Transport.Submit(MakeContext(), Command);
}

void HeadlessSessionLayer::SubmitToTransport(ISessionTransport &Transport,
                                             SessionUserId User,
                                             const EditorCommand &Command) {
  Transport.Submit(MakeContext(User), Command);
}


CommandContext HeadlessSessionLayer::MakeContext() const {
  return MakeContext(m_LocalUserId);
}

CommandContext HeadlessSessionLayer::MakeContext(SessionUserId User) const {
  return {
      .Session = m_SessionId,
      .User = User,
      .FrameIndex = Application::Get().GetFrameIndex(),
      .DeltaTimeSeconds = Application::Get().GetDeltaTime(),
  };
}
} // namespace Axiom
