#include "RemoteViewportWebSocketDispatch.h"

#include "HeadlessCommandProtocol.h"
#include "RemoteViewportGizmoController.h"
#include "RemoteViewportHttpRouter.h"
#include "RemoteViewportServer.h"
#include "RemoteViewportWebRtcSessionManager.h"

#include <App.h>
#include <Assets/SceneFile.h>

#include <algorithm>

namespace Axiom {
namespace {
struct RemoteViewportWebSocketUserData {
  uintptr_t ConnectionId{0};
};

using UwsWebSocket =
    uWS::WebSocket<false, true, RemoteViewportWebSocketUserData>;

bool HandleSetProperty(HeadlessSessionHost &Host, RemoteClientSession &Client,
                       const HeadlessCommand &Command) {
  if (!Command.PropertyVal.has_value()) {
    return false;
  }

  const auto &Name = Command.PropertyName;
  const auto &Val = *Command.PropertyVal;
  const auto &ObjId = Command.ObjectId;

  if (Name == "displayName") {
    if (const auto *S = std::get_if<std::string>(&Val)) {
      Host.SubmitRemoteCommand(
          Client.User,
          EditorCommand{RenameObjectCommand{.ObjectId = ObjId, .DisplayName = *S}});
      return true;
    }
  } else if (Name == "visible") {
    if (const auto *B = std::get_if<bool>(&Val)) {
      Host.SubmitRemoteCommand(
          Client.User,
          EditorCommand{SetObjectVisibilityCommand{.ObjectId = ObjId, .Visible = *B}});
      return true;
    }
  } else if (Name == "scriptClass") {
    if (const auto *S = std::get_if<std::string>(&Val)) {
      if (S->empty()) {
        Host.SubmitRemoteCommand(
            Client.User, EditorCommand{DetachScriptCommand{.ObjectId = ObjId}});
      } else {
        Host.SubmitRemoteCommand(
            Client.User, EditorCommand{AttachScriptCommand{
                             .ObjectId = ObjId,
                             .ScriptClassName = *S,
                         }});
      }
      return true;
    }
  } else if (Name == "physicsBodyType" || Name == "physicsColliderType" ||
             Name == "physicsBoxHalfExtents" || Name == "physicsSphereRadius" ||
             Name == "physicsMass" || Name == "physicsFriction" ||
             Name == "physicsRestitution") {
    const auto &DetailsById = Host.GetSessionModule()
                                  .GetSession()
                                  .GetState()
                                  .Scene.ObjectDetailsById;
    const auto It = DetailsById.find(ObjId);
    if (It == DetailsById.end() || !It->second.SupportsTransform) {
      return false;
    }

    EditorPhysicsProperties Physics =
        It->second.Physics.value_or(EditorPhysicsProperties{});
    if (Name == "physicsBodyType") {
      const auto *S = std::get_if<std::string>(&Val);
      if (S == nullptr) return false;
      if (*S == "none") Physics.BodyType = EditorPhysicsBodyType::None;
      else if (*S == "static") Physics.BodyType = EditorPhysicsBodyType::Static;
      else if (*S == "dynamic") Physics.BodyType = EditorPhysicsBodyType::Dynamic;
      else return false;
    } else if (Name == "physicsColliderType") {
      const auto *S = std::get_if<std::string>(&Val);
      if (S == nullptr) return false;
      if (*S == "none") Physics.ColliderType = EditorPhysicsColliderType::None;
      else if (*S == "box") Physics.ColliderType = EditorPhysicsColliderType::Box;
      else if (*S == "sphere") Physics.ColliderType = EditorPhysicsColliderType::Sphere;
      else return false;
    } else if (Name == "physicsBoxHalfExtents") {
      const auto *V = std::get_if<glm::vec3>(&Val);
      if (V == nullptr) return false;
      Physics.BoxHalfExtents = *V;
    } else if (Name == "physicsSphereRadius") {
      const auto *Number = std::get_if<float>(&Val);
      if (Number == nullptr) return false;
      Physics.SphereRadius = *Number;
    } else if (Name == "physicsMass") {
      const auto *Number = std::get_if<float>(&Val);
      if (Number == nullptr) return false;
      Physics.Mass = *Number;
    } else if (Name == "physicsFriction") {
      const auto *Number = std::get_if<float>(&Val);
      if (Number == nullptr) return false;
      Physics.Friction = *Number;
    } else if (Name == "physicsRestitution") {
      const auto *Number = std::get_if<float>(&Val);
      if (Number == nullptr) return false;
      Physics.Restitution = *Number;
    }

    Host.SubmitRemoteCommand(
        Client.User,
        EditorCommand{SetPhysicsPropertiesCommand{
            .ObjectId = ObjId,
            .Physics = Physics,
        }});
    return true;
  } else if (Name == "location" || Name == "rotationDegrees" || Name == "scale") {
    if (const auto *V = std::get_if<glm::vec3>(&Val)) {
      const auto &DetailsById = Host.GetSessionModule()
                                    .GetSession()
                                    .GetState()
                                    .Scene.ObjectDetailsById;
      const auto It = DetailsById.find(ObjId);
      if (It == DetailsById.end() || !It->second.Transform.has_value()) {
        return false;
      }
      const EditorTransformDetails &Current = *It->second.Transform;
      SetTransformCommand Cmd{
          .ObjectId = ObjId,
          .Location = Current.Location,
          .RotationDegrees = Current.RotationDegrees,
          .Scale = Current.Scale,
      };
      if (Name == "location") Cmd.Location = *V;
      else if (Name == "rotationDegrees") Cmd.RotationDegrees = *V;
      else Cmd.Scale = *V;
      Host.SubmitRemoteCommand(Client.User, EditorCommand{Cmd});
      return true;
    }
  }

  return false;
}
} // namespace

RemoteViewportWebSocketDispatch::RemoteViewportWebSocketDispatch(
    RemoteViewportServer &Server)
    : m_Server(Server) {}

size_t RemoteViewportWebSocketDispatch::GetActiveClientCount() const {
  std::scoped_lock Lock(m_WebSocketMutex);
  return m_WebSocketClients.size();
}

uint64_t RemoteViewportWebSocketDispatch::GetTotalWebSocketMessages() const {
  return m_TotalWebSocketMessages.load();
}

void RemoteViewportWebSocketDispatch::OnClientOpen(uintptr_t ConnectionId,
                                                   void *Socket) {
  {
    std::scoped_lock Lock(m_WebSocketMutex);
    m_WebSocketClients.push_back(
        {.ConnectionId = ConnectionId, .Socket = Socket, .IsOpen = true});
  }
  std::cout << SerializeConnected() << std::endl;
  SendTextMessage(ConnectionId,
                  SerializeReady(m_Server.m_Options.Width, m_Server.m_Options.Height));
  SendTextMessage(ConnectionId, SerializeConnected());
}

void RemoteViewportWebSocketDispatch::OnClientClose(uintptr_t ConnectionId) {
  bool Removed = false;
  {
    std::scoped_lock Lock(m_WebSocketMutex);
    auto It = std::find_if(m_WebSocketClients.begin(), m_WebSocketClients.end(),
                           [ConnectionId](const WebSocketClient &Client) {
                             return Client.ConnectionId == ConnectionId;
                           });
    if (It != m_WebSocketClients.end()) {
      It->IsOpen = false;
      m_WebSocketClients.erase(It);
      Removed = true;
    }
  }
  if (Removed) {
    std::cout << SerializeDisconnected() << std::endl;
  }
}

void RemoteViewportWebSocketDispatch::CloseAllClients() {
  std::scoped_lock Lock(m_WebSocketMutex);
  for (auto &Client : m_WebSocketClients) {
    Client.IsOpen = false;
  }
  m_WebSocketClients.clear();
}

void RemoteViewportWebSocketDispatch::BroadcastTextMessage(std::string Message) {
  std::vector<uintptr_t> Clients;
  {
    std::scoped_lock Lock(m_WebSocketMutex);
    for (const auto &Client : m_WebSocketClients) {
      if (Client.IsOpen) {
        Clients.push_back(Client.ConnectionId);
      }
    }
  }
  std::vector<uintptr_t> FailedClients;
  for (const uintptr_t ClientSocketValue : Clients) {
    if (!SendTextMessage(ClientSocketValue, Message)) {
      FailedClients.push_back(ClientSocketValue);
    }
  }
  for (const uintptr_t FailedClient : FailedClients) {
    OnClientClose(FailedClient);
  }
}

bool RemoteViewportWebSocketDispatch::SendTextMessage(
    uintptr_t ClientSocketValue, std::string_view Message) {
  void *SocketHandle = nullptr;
  {
    std::scoped_lock Lock(m_WebSocketMutex);
    const auto It = std::find_if(
        m_WebSocketClients.begin(), m_WebSocketClients.end(),
        [ClientSocketValue](const WebSocketClient &Client) {
          return Client.ConnectionId == ClientSocketValue && Client.IsOpen;
        });
    if (It == m_WebSocketClients.end()) {
      return false;
    }
    SocketHandle = It->Socket;
  }
  auto *Socket = static_cast<UwsWebSocket *>(SocketHandle);
  std::scoped_lock Lock(m_SendMutex);
  return Socket->send(Message, uWS::OpCode::TEXT) != UwsWebSocket::DROPPED;
}

bool RemoteViewportWebSocketDispatch::SendBinaryMessage(
    uintptr_t ClientSocketValue, const void *Data, size_t Size) {
  void *SocketHandle = nullptr;
  {
    std::scoped_lock Lock(m_WebSocketMutex);
    const auto It = std::find_if(
        m_WebSocketClients.begin(), m_WebSocketClients.end(),
        [ClientSocketValue](const WebSocketClient &Client) {
          return Client.ConnectionId == ClientSocketValue && Client.IsOpen;
        });
    if (It == m_WebSocketClients.end()) {
      return false;
    }
    SocketHandle = It->Socket;
  }
  auto *Socket = static_cast<UwsWebSocket *>(SocketHandle);
  const std::string_view Payload(static_cast<const char *>(Data), Size);
  std::scoped_lock Lock(m_SendMutex);
  return Socket->send(Payload, uWS::OpCode::BINARY) != UwsWebSocket::DROPPED;
}

bool RemoteViewportWebSocketDispatch::HandleWebSocketMessage(
    uintptr_t ClientSocketValue, std::string_view Payload) {
  m_TotalWebSocketMessages.fetch_add(1);
  std::string Error;
  const auto Command = ParseRemoteViewportCommand(Payload, Error);
  if (!Command.has_value()) {
    return false;
  }

  switch (Command->Type) {
  case HeadlessCommandType::SetViewMode:
    m_Server.m_Host.SetRemoteViewMode(Command->ViewMode);
    return true;
  case HeadlessCommandType::SetShowColliders:
    m_Server.m_Host.SetRemoteShowColliders(Command->ShowColliders);
    return true;
  case HeadlessCommandType::DropMesh:
    m_Server.m_GizmoController->HandleMeshDropCommand(
        m_Server.m_Host.GetSessionModule().GetLocalUserId(), *Command);
    return true;
  case HeadlessCommandType::DropTexture:
    m_Server.m_GizmoController->HandleTextureDropCommand(
        m_Server.m_Host.GetSessionModule().GetLocalUserId(), *Command);
    return true;
  case HeadlessCommandType::PlaceActor:
    m_Server.m_GizmoController->HandlePlaceActorCommand(
        m_Server.m_Host.GetSessionModule().GetLocalUserId(), *Command);
    return true;
  case HeadlessCommandType::ListAssets:
    SendTextMessage(ClientSocketValue,
                    SerializeAssetList(m_Server.m_HttpRouter->CollectVisibleAssets()));
    return true;
  case HeadlessCommandType::GetSchema: {
    const auto &DetailsById = m_Server.m_Host.GetSessionModule()
                                  .GetSession()
                                  .GetState()
                                  .Scene.ObjectDetailsById;
    const auto It = DetailsById.find(Command->ObjectId);
    if (It != DetailsById.end()) {
      SendTextMessage(ClientSocketValue, SerializeObjectSchema(It->second));
    }
    return true;
  }
  case HeadlessCommandType::SaveScene: {
    const Assets::LocalAssetSource ContentDir{
        m_Server.m_HttpRouter->GetActiveContentDir()};
    const auto ScenePath = ContentDir.ResolveRelative("scene.json");
    const bool Ok = Assets::SaveSceneToFile(
        ScenePath, m_Server.m_Host.GetSessionModule().GetSession().GetState().Scene);
    SendTextMessage(ClientSocketValue, SerializeSaveResult(Ok));
    return true;
  }
  case HeadlessCommandType::Quit:
    m_Server.m_StopRequested.store(true);
    m_Server.m_Host.RequestClose();
    BroadcastTextMessage(SerializeShutdown());
    return true;
  default:
    return false;
  }
}

bool RemoteViewportWebSocketDispatch::HandleClientWebRtcMessage(
    std::string_view ClientId, std::string_view Payload) {
  m_TotalWebSocketMessages.fetch_add(1);
  std::string Error;
  const auto Command = ParseRemoteViewportCommand(Payload, Error);
  if (!Command.has_value()) {
    return false;
  }

  auto Client = m_Server.m_WebRtcSessions->FindClientSession(ClientId);
  if (Client == nullptr) {
    return false;
  }
  m_Server.m_WebRtcSessions->TouchClientSession(Client->ClientId);

  switch (Command->Type) {
  case HeadlessCommandType::SetViewMode:
    m_Server.m_Host.SetRemoteViewMode(Client->User, Command->ViewMode);
    return true;
  case HeadlessCommandType::SetShowColliders:
    m_Server.m_Host.SetRemoteShowColliders(Client->User, Command->ShowColliders);
    return true;
  case HeadlessCommandType::SetLookActive:
  case HeadlessCommandType::SetViewportCameraPose:
  case HeadlessCommandType::SetCameraProjection:
  case HeadlessCommandType::UpdateViewportCamera:
  case HeadlessCommandType::SelectObject:
  case HeadlessCommandType::RenameObject:
  case HeadlessCommandType::SetObjectVisibility:
  case HeadlessCommandType::CreateObject:
  case HeadlessCommandType::DuplicateObject:
  case HeadlessCommandType::DeleteObject:
  case HeadlessCommandType::ReparentObject:
  case HeadlessCommandType::SetTransform:
  case HeadlessCommandType::AttachScript:
  case HeadlessCommandType::DetachScript:
  case HeadlessCommandType::PlaySession:
  case HeadlessCommandType::PauseSession:
  case HeadlessCommandType::ResumeSession:
  case HeadlessCommandType::StopSession:
  case HeadlessCommandType::SetMeshAsset:
  case HeadlessCommandType::SetLightProperties:
  case HeadlessCommandType::SetMaterialProperties:
  case HeadlessCommandType::SetMaterialTexture:
  case HeadlessCommandType::SetWorldSettings:
    m_Server.m_Host.SubmitRemoteCommand(Client->User, Command->EditorPayload);
    return true;
  case HeadlessCommandType::DropMesh:
    m_Server.m_GizmoController->HandleMeshDropCommand(Client->User, *Command);
    return true;
  case HeadlessCommandType::DropTexture:
    m_Server.m_GizmoController->HandleTextureDropCommand(Client->User, *Command);
    return true;
  case HeadlessCommandType::PlaceActor:
    m_Server.m_GizmoController->HandlePlaceActorCommand(Client->User, *Command);
    return true;
  case HeadlessCommandType::ReloadScripts:
    m_Server.m_Host.ReloadUserScripts();
    if (Client->WebRtcSession != nullptr) {
      Client->WebRtcSession->SendReliableMessage(
          "{\"type\":\"scripts_reloaded\"}");
    }
    return true;
  case HeadlessCommandType::Heartbeat: {
    const EditorUserPresence *Presence =
        m_Server.m_Host.GetSessionModule().GetSession().FindPresence(Client->User);
    if (Presence != nullptr && Presence->State == EditorUserPresenceState::Away) {
      m_Server.m_Host.GetSessionModule().GetSession().SetPresenceState(
          Client->User, EditorUserPresenceState::Connected);
    }
    return true;
  }
  case HeadlessCommandType::ListAssets:
    if (Client->WebRtcSession != nullptr) {
      Client->WebRtcSession->SendReliableMessage(
          SerializeAssetList(m_Server.m_HttpRouter->CollectVisibleAssets()));
    }
    return true;
  case HeadlessCommandType::GetSchema: {
    const auto &DetailsById = m_Server.m_Host.GetSessionModule()
                                  .GetSession()
                                  .GetState()
                                  .Scene.ObjectDetailsById;
    const auto It = DetailsById.find(Command->ObjectId);
    if (It != DetailsById.end() && Client->WebRtcSession != nullptr) {
      Client->WebRtcSession->SendReliableMessage(SerializeObjectSchema(It->second));
    }
    return true;
  }
  case HeadlessCommandType::SaveScene: {
    const Assets::LocalAssetSource ContentDir{
        m_Server.m_HttpRouter->GetActiveContentDir()};
    const auto ScenePath = ContentDir.ResolveRelative("scene.json");
    const bool Ok = Assets::SaveSceneToFile(
        ScenePath, m_Server.m_Host.GetSessionModule().GetSession().GetState().Scene);
    if (Client->WebRtcSession != nullptr) {
      Client->WebRtcSession->SendReliableMessage(SerializeSaveResult(Ok));
    }
    return true;
  }
  case HeadlessCommandType::SetProperty:
    return HandleSetProperty(m_Server.m_Host, *Client, *Command);
  case HeadlessCommandType::SetGizmoMode:
  case HeadlessCommandType::SetGridSnap:
  case HeadlessCommandType::GizmoHover:
  case HeadlessCommandType::GizmoDragStart:
  case HeadlessCommandType::GizmoDragUpdate:
  case HeadlessCommandType::GizmoDragEnd:
    return m_Server.m_GizmoController->HandleRemoteClientCommand(*Client,
                                                                 *Command);
  case HeadlessCommandType::Quit:
    m_Server.m_StopRequested.store(true);
    m_Server.m_Host.RequestClose();
    BroadcastTextMessage(SerializeShutdown());
    return true;
  default:
    return false;
  }
}
} // namespace Axiom
