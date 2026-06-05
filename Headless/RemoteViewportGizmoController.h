#pragma once

#include "HeadlessCommandProtocol.h"

#include <GizmoHitTest.h>
#include <Renderer/RenderScene.h>

#include <string>

namespace Axiom {
class RemoteViewportServer;
struct RemoteClientSession;

struct ActiveGizmoDrag {
  GizmoDragState Math;
  std::string ObjectId;
  glm::vec3 StartRotDeg{0.0f};
  glm::vec3 StartScale{1.0f};
  GizmoMode Mode{GizmoMode::Translate};
  float GizmoScaleAtDragStart{1.0f};
};

class RemoteViewportGizmoController {
public:
  explicit RemoteViewportGizmoController(RemoteViewportServer &Server);

  void HandleTextureDropCommand(SessionUserId User,
                                const HeadlessCommand &Command);
  void HandleMeshDropCommand(SessionUserId User,
                             const HeadlessCommand &Command);
  void HandlePlaceActorCommand(SessionUserId User,
                               const HeadlessCommand &Command);
  bool HandleRemoteClientCommand(RemoteClientSession &Client,
                                 const HeadlessCommand &Command);

private:
  RemoteViewportServer &m_Server;
};
} // namespace Axiom
