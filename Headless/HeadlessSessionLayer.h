#pragma once

#include <Core/Layer.h>

#include <Remote/SessionTransport.h>
#include <Renderer/Material.h>
#include <Renderer/Mesh.h>
#include <Renderer/RenderScene.h>
#include <Session/EditorSceneRendererAdapter.h>
#include <Session/EditorSession.h>

#include <functional>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace Axiom {
struct HeadlessRenderViewState;
class ScriptHost;

class HeadlessSessionLayer final : public Layer {
public:
  using RenderViewResolver =
      std::function<std::optional<HeadlessRenderViewState>()>;

  HeadlessSessionLayer();

  void OnAttach() override;
  void OnUpdate() override;
  void OnRender() override;

  bool LoadStartupSceneIntoSession();
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
  void SetPresenceMarkerMeshForTesting(MeshRef Mesh) { m_PresenceMarkerMesh = std::move(Mesh); }
  void SetScriptHost(ScriptHost *Host) { m_ScriptHost = Host; }
  EditorSession &GetSession() { return m_Session; }
  SessionUserId GetLocalUserId() const { return m_LocalUserId; }

  void SetGizmoHoveredAxis(SessionUserId User, int Axis);
  int GetGizmoHoveredAxis(SessionUserId User) const;
  void SetGizmoMode(SessionUserId User, GizmoMode Mode);
  GizmoMode GetGizmoMode(SessionUserId User) const;
  std::vector<RenderMeshSubmission>
  BuildPresenceOverlaySubmissions(SessionUserId RenderUser) const;

private:
  MaterialInstanceRef GetOrCreatePresenceMaterial(SessionUserId User) const;
  CommandContext MakeContext() const;
  CommandContext MakeContext(SessionUserId User) const;

  SessionId m_SessionId{1};
  SessionUserId m_LocalUserId{1};
  EditorSession m_Session;
  ScriptHost *m_ScriptHost{nullptr};
  EditorSceneRendererAdapter *m_RendererAdapter{nullptr};
  MeshRef m_PresenceMarkerMesh;
  RenderViewResolver m_RenderViewResolver;
  mutable std::unordered_map<uint64_t, MaterialInstanceRef> m_PresenceMaterials;
  mutable std::mutex m_GizmoHoverMutex;
  std::unordered_map<uint64_t, int> m_GizmoHoveredAxisByUser;
  mutable std::mutex m_GizmoModeMutex;
  std::unordered_map<uint64_t, GizmoMode> m_GizmoModeByUser;
};
} // namespace Axiom
