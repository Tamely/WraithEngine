#pragma once

#include <Core/Layer.h>

#include <Remote/SessionTransport.h>
#include <Renderer/Material.h>
#include <Renderer/Mesh.h>
#include <Session/EditorSceneRendererAdapter.h>
#include <Session/EditorSession.h>

#include <functional>
#include <optional>
#include <unordered_map>

namespace Axiom {
struct HeadlessRenderViewState;

class HeadlessSessionLayer final : public Layer {
public:
  using RenderFrameObserver =
      std::function<void(uint64_t FrameIndex, SessionUserId User)>;
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
  void SetRenderFrameObserver(RenderFrameObserver Observer) {
    m_RenderFrameObserver = std::move(Observer);
  }
  void SetPresenceMarkerMeshForTesting(MeshRef Mesh) { m_PresenceMarkerMesh = std::move(Mesh); }
  EditorSession &GetSession() { return m_Session; }
  SessionUserId GetLocalUserId() const { return m_LocalUserId; }
  std::vector<RenderMeshSubmission>
  BuildPresenceOverlaySubmissions(SessionUserId RenderUser) const;

private:
  MaterialInstanceRef GetOrCreatePresenceMaterial(SessionUserId User) const;
  CommandContext MakeContext() const;
  CommandContext MakeContext(SessionUserId User) const;

  SessionId m_SessionId{1};
  SessionUserId m_LocalUserId{1};
  EditorSession m_Session;
  EditorSceneRendererAdapter *m_RendererAdapter{nullptr};
  MeshRef m_PresenceMarkerMesh;
  RenderViewResolver m_RenderViewResolver;
  RenderFrameObserver m_RenderFrameObserver;
  mutable std::unordered_map<uint64_t, MaterialInstanceRef> m_PresenceMaterials;
};
} // namespace Axiom
