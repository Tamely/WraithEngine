#include "RemoteViewportGizmoController.h"

#include "RemoteViewportGridSnap.h"
#include "RemoteViewportServer.h"
#include "RemoteViewportWebRtcSessionManager.h"

#include <Session/MeshPicking.h>

#include <algorithm>

namespace Axiom {
RemoteViewportGizmoController::RemoteViewportGizmoController(
    RemoteViewportServer &Server)
    : m_Server(Server) {}

void RemoteViewportGizmoController::HandleTextureDropCommand(
    SessionUserId User, const HeadlessCommand &Command) {
  if (Command.TextureAssetPath.empty()) {
    return;
  }

  const EditorSession &Session = m_Server.m_Host.GetSessionModule().GetSession();
  const EditorViewportState *Viewport = Session.FindViewport(User);
  if (Viewport == nullptr) {
    return;
  }

  const std::string HitId = HitTestMeshes(
      Viewport->Camera, m_Server.m_Options.Width, m_Server.m_Options.Height,
      Command.MousePosition, Session.GetState().Scene.MeshInstances);
  if (HitId.empty()) {
    return;
  }

  m_Server.m_Host.SubmitRemoteCommand(
      User, EditorCommand{SetMaterialTextureCommand{
                .ObjectId = HitId,
                .TextureAssetPath = Command.TextureAssetPath,
            }});
}

void RemoteViewportGizmoController::HandleMeshDropCommand(
    SessionUserId User, const HeadlessCommand &Command) {
  if (Command.MeshAssetPath.empty()) {
    return;
  }

  const EditorSession &Session = m_Server.m_Host.GetSessionModule().GetSession();
  const EditorViewportState *Viewport = Session.FindViewport(User);
  if (Viewport == nullptr) {
    return;
  }

  const glm::vec3 SpawnLocation = ResolveViewportDropPosition(
      Viewport->Camera, m_Server.m_Options.Width, m_Server.m_Options.Height,
      Command.MousePosition, Session.GetState().Scene.MeshInstances);

  m_Server.m_Host.SubmitRemoteCommand(
      User, EditorCommand{CreateMeshObjectCommand{
                .AssetPath = Command.MeshAssetPath,
                .Location = SpawnLocation,
                .RotationDegrees = glm::vec3(0.0f),
                .Scale = glm::vec3(1.0f),
            }});
}

void RemoteViewportGizmoController::HandlePlaceActorCommand(
    SessionUserId User, const HeadlessCommand &Command) {
  const EditorSession &Session = m_Server.m_Host.GetSessionModule().GetSession();
  const EditorViewportState *Viewport = Session.FindViewport(User);
  if (Viewport == nullptr) {
    return;
  }

  glm::vec2 MousePos = Command.MousePosition;
  if (MousePos.x < 0.0f || MousePos.y < 0.0f) {
    MousePos = {static_cast<float>(m_Server.m_Options.Width) * 0.5f,
                static_cast<float>(m_Server.m_Options.Height) * 0.5f};
  }

  const glm::vec3 SpawnLocation = ResolveViewportDropPosition(
      Viewport->Camera, m_Server.m_Options.Width, m_Server.m_Options.Height,
      MousePos, Session.GetState().Scene.MeshInstances);

  m_Server.m_Host.SubmitRemoteCommand(
      User, EditorCommand{PlaceActorCommand{
                .ChildTemplateId = Command.PlaceActorTemplateId,
                .ChildMeshAssetPath = Command.PlaceActorMeshAssetPath,
                .Location = SpawnLocation,
            }});
}

bool RemoteViewportGizmoController::HandleRemoteClientCommand(
    RemoteClientSession &Client, const HeadlessCommand &Command) {
  switch (Command.Type) {
  case HeadlessCommandType::SetGizmoMode:
    Client.CurrentGizmoMode = Command.Mode;
    m_Server.m_Host.GetSessionModule().SetGizmoMode(Client.User, Command.Mode);
    return true;
  case HeadlessCommandType::SetGridSnap:
    Client.GridSnap.Enabled = Command.Enabled;
    Client.GridSnap.TranslationStep = Command.TranslationStep;
    Client.GridSnap.RotationStepDegrees = Command.RotationStepDegrees;
    Client.GridSnap.ScaleStep = Command.ScaleStep;
    m_Server.m_GridSnap->Sanitize(Client.GridSnap);
    return true;
  case HeadlessCommandType::GizmoHover: {
    if (m_Server.m_Host.GetSessionModule().GetSession().GetRuntimeState() !=
        EditorRuntimeState::Edit) {
      m_Server.m_Host.GetSessionModule().SetGizmoHoveredAxis(Client.User, -1);
      return true;
    }
    if (Client.GizmoDrag.has_value()) {
      return true;
    }
    const EditorSession &Session = m_Server.m_Host.GetSessionModule().GetSession();
    const EditorViewportState *Viewport = Session.FindViewport(Client.User);
    const EditorObjectDetails *Selected =
        Session.FindSelectedObjectDetails(Client.User);
    const auto *HoverTD = (Selected != nullptr && Selected->SupportsTransform)
                              ? (Selected->WorldTransform.has_value()
                                     ? &*Selected->WorldTransform
                                     : (Selected->Transform.has_value()
                                            ? &*Selected->Transform
                                            : nullptr))
                              : nullptr;
    if (Viewport != nullptr && HoverTD != nullptr) {
      const float GizmoScale = ComputeGizmoScale(
          Viewport->Camera, HoverTD->Location, m_Server.m_Options.Width,
          m_Server.m_Options.Height);
      const int Axis =
          (Client.CurrentGizmoMode == GizmoMode::Rotate)
              ? HitTestGizmoRings(Viewport->Camera, m_Server.m_Options.Width,
                                  m_Server.m_Options.Height,
                                  Command.MousePosition, HoverTD->Location,
                                  GizmoScale)
              : HitTestGizmoAxes(Viewport->Camera, m_Server.m_Options.Width,
                                 m_Server.m_Options.Height,
                                 Command.MousePosition, HoverTD->Location,
                                 GizmoScale);
      m_Server.m_Host.GetSessionModule().SetGizmoHoveredAxis(Client.User, Axis);
    } else {
      m_Server.m_Host.GetSessionModule().SetGizmoHoveredAxis(Client.User, -1);
    }
    return true;
  }
  case HeadlessCommandType::GizmoDragStart: {
    if (m_Server.m_Host.GetSessionModule().GetSession().GetRuntimeState() !=
        EditorRuntimeState::Edit) {
      return true;
    }
    if (Client.GizmoDrag.has_value()) {
      return true;
    }

    EditorSession &Session = m_Server.m_Host.GetSessionModule().GetSession();
    const EditorViewportState *Viewport = Session.FindViewport(Client.User);
    if (Viewport == nullptr) {
      return true;
    }
    const EditorObjectDetails *Selected =
        Session.FindSelectedObjectDetails(Client.User);
    const auto *DragTD =
        (Selected != nullptr && Selected->SupportsTransform &&
         !Selected->TransformReadOnly)
            ? (Selected->WorldTransform.has_value()
                   ? &*Selected->WorldTransform
                   : (Selected->Transform.has_value() ? &*Selected->Transform
                                                      : nullptr))
            : nullptr;

    if (DragTD != nullptr) {
      const glm::vec3 &ObjPos = DragTD->Location;
      const float GizmoScale = ComputeGizmoScale(
          Viewport->Camera, ObjPos, m_Server.m_Options.Width,
          m_Server.m_Options.Height);
      if (Client.CurrentGizmoMode == GizmoMode::Rotate) {
        auto DragState = BeginGizmoRotateDrag(
            Viewport->Camera, m_Server.m_Options.Width, m_Server.m_Options.Height,
            Command.MousePosition, ObjPos, GizmoScale, ObjPos);
        if (DragState.has_value()) {
          Client.GizmoDrag = ActiveGizmoDrag{
              .Math = *DragState,
              .ObjectId = Selected->ObjectId,
              .StartRotDeg = DragTD->RotationDegrees,
              .StartScale = DragTD->Scale,
              .Mode = GizmoMode::Rotate,
              .GizmoScaleAtDragStart = GizmoScale,
          };
          Session.AcquireLock(Selected->ObjectId, Client.User);
          m_Server.m_Host.GetSessionModule().SetGizmoHoveredAxis(
              Client.User, DragState->Axis);
          return true;
        }
      } else {
        auto DragState = BeginGizmoDrag(
            Viewport->Camera, m_Server.m_Options.Width, m_Server.m_Options.Height,
            Command.MousePosition, ObjPos, GizmoScale, ObjPos);
        if (DragState.has_value()) {
          Client.GizmoDrag = ActiveGizmoDrag{
              .Math = *DragState,
              .ObjectId = Selected->ObjectId,
              .StartRotDeg = DragTD->RotationDegrees,
              .StartScale = DragTD->Scale,
              .Mode = Client.CurrentGizmoMode,
              .GizmoScaleAtDragStart = GizmoScale,
          };
          Session.AcquireLock(Selected->ObjectId, Client.User);
          m_Server.m_Host.GetSessionModule().SetGizmoHoveredAxis(
              Client.User, DragState->Axis);
          return true;
        }
      }
    }

    const auto Hit = ResolveViewportSelectionHit(
        Viewport->Camera, m_Server.m_Options.Width, m_Server.m_Options.Height,
        Command.MousePosition, Session.GetState().Scene.MeshInstances,
        m_Server.m_Host.GetSessionModule().BuildLightBillboards());
    if (Hit.has_value() && !Hit->ObjectId.empty()) {
      std::string SelectId = Hit->ObjectId;
      if (const EditorObjectDetails *Picked = Session.FindObjectDetails(SelectId);
          Picked != nullptr && Picked->IsGeneratedAssetChild &&
          Picked->GeneratedFromAssetRootId.has_value()) {
        SelectId = *Picked->GeneratedFromAssetRootId;
      }
      m_Server.m_Host.SubmitRemoteCommand(
          Client.User, EditorCommand{SelectObjectCommand{.ObjectId = SelectId}});
    }
    return true;
  }
  case HeadlessCommandType::GizmoDragUpdate:
  case HeadlessCommandType::GizmoDragEnd: {
    const bool IsEnd = Command.Type == HeadlessCommandType::GizmoDragEnd;
    if (m_Server.m_Host.GetSessionModule().GetSession().GetRuntimeState() !=
        EditorRuntimeState::Edit) {
      if (Client.GizmoDrag.has_value()) {
        EditorSession &Session = m_Server.m_Host.GetSessionModule().GetSession();
        const std::string DragObjectId = Client.GizmoDrag->ObjectId;
        Client.GizmoDrag.reset();
        Session.ReleaseLock(DragObjectId, Client.User);
        m_Server.m_Host.GetSessionModule().SetGizmoHoveredAxis(Client.User, -1);
      }
      return true;
    }
    if (!Client.GizmoDrag.has_value()) {
      return true;
    }

    EditorSession &Session = m_Server.m_Host.GetSessionModule().GetSession();
    const EditorViewportState *Viewport = Session.FindViewport(Client.User);
    if (Viewport != nullptr) {
      const ActiveGizmoDrag &Drag = *Client.GizmoDrag;
      glm::vec3 Location = Drag.Math.ObjectStartPos;
      glm::vec3 RotDeg = Drag.StartRotDeg;
      glm::vec3 Scale = Drag.StartScale;
      if (Drag.Mode == GizmoMode::Translate) {
        Location = UpdateGizmoDrag(
            Drag.Math, Viewport->Camera, m_Server.m_Options.Width,
            m_Server.m_Options.Height, Command.MousePosition.x,
            Command.MousePosition.y);
      } else if (Drag.Mode == GizmoMode::Scale) {
        const glm::vec3 NewPosTmp =
            UpdateGizmoDrag(Drag.Math, Viewport->Camera, m_Server.m_Options.Width,
                            m_Server.m_Options.Height, Command.MousePosition.x,
                            Command.MousePosition.y);
        const float DeltaT =
            glm::dot(NewPosTmp - Drag.Math.ObjectStartPos, Drag.Math.AxisDir);
        const float Factor =
            std::max(0.001f, 1.0f + DeltaT /
                                  std::max(0.001f, Drag.GizmoScaleAtDragStart));
        Scale[Drag.Math.Axis] = Drag.StartScale[Drag.Math.Axis] * Factor;
      } else {
        const float DeltaDeg = UpdateGizmoRotateDrag(
            Drag.Math, Viewport->Camera, m_Server.m_Options.Width,
            m_Server.m_Options.Height, Command.MousePosition.x,
            Command.MousePosition.y);
        RotDeg[Drag.Math.Axis] = Drag.StartRotDeg[Drag.Math.Axis] + DeltaDeg;
      }
      m_Server.m_GridSnap->Apply(Client.GridSnap, Drag.Mode, Drag.Math.Axis,
                                 Location, RotDeg, Scale);
      EditorCommand Cmd;
      Cmd.Payload = SetTransformCommand{
          .ObjectId = Drag.ObjectId,
          .Location = Location,
          .RotationDegrees = RotDeg,
          .Scale = Scale,
      };
      m_Server.m_Host.SubmitRemoteCommand(Client.User, Cmd);
    }

    if (IsEnd) {
      const std::string DragObjectId = Client.GizmoDrag->ObjectId;
      Client.GizmoDrag.reset();
      Session.ReleaseLock(DragObjectId, Client.User);
      m_Server.m_Host.GetSessionModule().SetGizmoHoveredAxis(Client.User, -1);
    }
    return true;
  }
  default:
    return false;
  }
}
} // namespace Axiom
