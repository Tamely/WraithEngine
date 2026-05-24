#pragma once

#include <Renderer/Material.h>
#include <Renderer/Mesh.h>
#include <Renderer/RenderScene.h>
#include <Session/EditorSession.h>

#include <mutex>
#include <unordered_map>

namespace Axiom {
class HeadlessOverlayModule {
public:
  explicit HeadlessOverlayModule(EditorSession &Session);

  void Initialize();
  void SetPresenceMarkerMeshForTesting(MeshRef Mesh);
  void SetColliderMeshesForTesting(MeshRef BoxMesh, MeshRef SphereMesh);

  void SetGizmoHoveredAxis(SessionUserId User, int Axis);
  int GetGizmoHoveredAxis(SessionUserId User) const;
  void SetGizmoMode(SessionUserId User, GizmoMode Mode);
  GizmoMode GetGizmoMode(SessionUserId User) const;

  std::vector<LightBillboardOverlay> BuildLightBillboards() const;
  std::vector<RenderMeshSubmission> BuildColliderOverlaySubmissions() const;
  std::vector<RenderMeshSubmission>
  BuildPresenceOverlaySubmissions(SessionUserId RenderUser) const;

private:
  MaterialInstanceRef GetOrCreatePresenceMaterial(SessionUserId User) const;
  MaterialInstanceRef
  GetOrCreateColliderMaterial(EditorPhysicsBodyType BodyType) const;

  EditorSession &m_Session;
  MeshRef m_PresenceMarkerMesh;
  MeshRef m_ColliderBoxMesh;
  MeshRef m_ColliderSphereMesh;
  mutable std::unordered_map<uint64_t, MaterialInstanceRef> m_PresenceMaterials;
  mutable std::unordered_map<int, MaterialInstanceRef> m_ColliderMaterials;
  mutable std::mutex m_GizmoHoverMutex;
  std::unordered_map<uint64_t, int> m_GizmoHoveredAxisByUser;
  mutable std::mutex m_GizmoModeMutex;
  std::unordered_map<uint64_t, GizmoMode> m_GizmoModeByUser;
};
} // namespace Axiom
