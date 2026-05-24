#pragma once

#include <Core/Layer.h>

#include <Remote/SessionTransport.h>
#include <Renderer/Material.h>
#include <Renderer/Mesh.h>
#include <Renderer/RenderScene.h>
#include <Session/EditorSceneRendererAdapter.h>
#include <Session/EditorSession.h>

#include "HeadlessOverlayModule.h"

#include <functional>
#include <optional>

namespace Axiom {
struct HeadlessRenderViewState;

class HeadlessSessionLayer final : public Layer {
public:
  using RenderViewResolver =
      std::function<std::optional<HeadlessRenderViewState>()>;

  HeadlessSessionLayer();

  void OnAttach() override;
  void OnUpdate() override;
  void OnRender() override;

  bool LoadStartupSceneIntoSession();
  bool LoadStartupSceneIntoSession(const std::filesystem::path &ContentDir);
  void Submit(const EditorCommand &Command);
  void SubmitToTransport(ISessionTransport &Transport,
                         const EditorCommand &Command);
  void SubmitToTransport(ISessionTransport &Transport, SessionUserId User,
                         const EditorCommand &Command);
  void SetSharedRendererAdapter(EditorSceneRendererAdapter *RendererAdapter) {
    m_RendererAdapter = RendererAdapter;
  }
  void SetRenderViewResolver(RenderViewResolver Resolver) {
    m_RenderViewResolver = std::move(Resolver);
  }
  void SetPresenceMarkerMeshForTesting(MeshRef Mesh) {
    m_OverlayModule.SetPresenceMarkerMeshForTesting(std::move(Mesh));
  }
  void SetColliderMeshesForTesting(MeshRef BoxMesh, MeshRef SphereMesh) {
    m_OverlayModule.SetColliderMeshesForTesting(std::move(BoxMesh),
                                                std::move(SphereMesh));
  }
  EditorSession &GetSession() { return m_Session; }
  SessionUserId GetLocalUserId() const { return m_LocalUserId; }

  void SetGizmoHoveredAxis(SessionUserId User, int Axis) {
    m_OverlayModule.SetGizmoHoveredAxis(User, Axis);
  }
  int GetGizmoHoveredAxis(SessionUserId User) const {
    return m_OverlayModule.GetGizmoHoveredAxis(User);
  }
  void SetGizmoMode(SessionUserId User, GizmoMode Mode) {
    m_OverlayModule.SetGizmoMode(User, Mode);
  }
  GizmoMode GetGizmoMode(SessionUserId User) const {
    return m_OverlayModule.GetGizmoMode(User);
  }
  std::vector<LightBillboardOverlay> BuildLightBillboards() const {
    return m_OverlayModule.BuildLightBillboards();
  }
  std::vector<RenderMeshSubmission> BuildColliderOverlaySubmissions() const {
    return m_OverlayModule.BuildColliderOverlaySubmissions();
  }
  std::vector<RenderMeshSubmission>
  BuildPresenceOverlaySubmissions(SessionUserId RenderUser) const {
    return m_OverlayModule.BuildPresenceOverlaySubmissions(RenderUser);
  }

private:
  CommandContext MakeContext() const;
  CommandContext MakeContext(SessionUserId User) const;

  SessionId m_SessionId{1};
  SessionUserId m_LocalUserId{1};
  EditorSession m_Session;
  HeadlessOverlayModule m_OverlayModule;
  EditorSceneRendererAdapter *m_RendererAdapter{nullptr};
  RenderViewResolver m_RenderViewResolver;
};
} // namespace Axiom
