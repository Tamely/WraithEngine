#pragma once

#include <Core/Layer.h>

#include <Remote/SessionTransport.h>
#include <Renderer/Material.h>
#include <Renderer/Mesh.h>
#include <Session/EditorSession.h>

#include <unordered_map>

namespace Axiom {
class HeadlessSessionLayer final : public Layer {
public:
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
  void SetActiveRenderUser(SessionUserId User) { m_ActiveRenderUserId = User; }
  void SetPresenceMarkerMeshForTesting(MeshRef Mesh) { m_PresenceMarkerMesh = std::move(Mesh); }
  EditorSession &GetSession() { return m_Session; }
  SessionUserId GetLocalUserId() const { return m_LocalUserId; }
  std::vector<RenderMeshSubmission> BuildPresenceOverlaySubmissions() const;

private:
  MaterialInstanceRef GetOrCreatePresenceMaterial(SessionUserId User) const;
  CommandContext MakeContext() const;
  CommandContext MakeContext(SessionUserId User) const;

  SessionId m_SessionId{1};
  SessionUserId m_LocalUserId{1};
  SessionUserId m_ActiveRenderUserId{1};
  EditorSession m_Session;
  MeshRef m_PresenceMarkerMesh;
  mutable std::unordered_map<uint64_t, MaterialInstanceRef> m_PresenceMaterials;
};
} // namespace Axiom
