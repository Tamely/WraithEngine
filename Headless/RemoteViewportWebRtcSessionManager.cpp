#include "RemoteViewportWebRtcSessionManager.h"

#include "HeadlessCommandProtocol.h"
#include "RemoteViewportHttpRouter.h"
#include "RemoteViewportServer.h"
#include "RemoteViewportWebSocketDispatch.h"

#include <Renderer/VideoEncoderFactory.h>
#include <Session/EditorSession.h>

#include <algorithm>
#include <cctype>
#include <memory>
#include <sstream>

namespace Axiom {
namespace {
constexpr std::string_view ClientIdHeaderName = "X-Axiom-Client-Id";

std::string Trim(std::string_view Value) {
  while (!Value.empty() &&
         std::isspace(static_cast<unsigned char>(Value.front())) != 0) {
    Value.remove_prefix(1);
  }
  while (!Value.empty() &&
         std::isspace(static_cast<unsigned char>(Value.back())) != 0) {
    Value.remove_suffix(1);
  }
  return std::string(Value);
}

bool EqualsCaseInsensitive(std::string_view Left, std::string_view Right) {
  if (Left.size() != Right.size()) {
    return false;
  }
  for (size_t Index = 0; Index < Left.size(); ++Index) {
    if (std::tolower(static_cast<unsigned char>(Left[Index])) !=
        std::tolower(static_cast<unsigned char>(Right[Index]))) {
      return false;
    }
  }
  return true;
}

std::optional<std::string> FindHeaderValue(std::string_view HeaderBlock,
                                           std::string_view HeaderName) {
  size_t LineStart = 0;
  while (LineStart < HeaderBlock.size()) {
    const size_t LineEnd = HeaderBlock.find("\r\n", LineStart);
    const std::string_view Line =
        HeaderBlock.substr(LineStart, LineEnd == std::string_view::npos
                                          ? std::string_view::npos
                                          : LineEnd - LineStart);
    const size_t Colon = Line.find(':');
    if (Colon != std::string_view::npos &&
        EqualsCaseInsensitive(Trim(Line.substr(0, Colon)), HeaderName)) {
      return Trim(Line.substr(Colon + 1));
    }
    if (LineEnd == std::string_view::npos) {
      break;
    }
    LineStart = LineEnd + 2;
  }
  return std::nullopt;
}

std::string GenerateClientId() {
  static std::atomic<uint64_t> Counter{1};
  const uint64_t Value = Counter.fetch_add(1);
  std::ostringstream Stream;
  Stream << "client-" << Value;
  return Stream.str();
}
} // namespace

struct RemoteClientSession::PacketOutput final : IEncodedVideoPacketOutput {
  PacketOutput(RemoteViewportWebRtcSessionManager &ManagerIn,
               std::string ClientIdIn)
      : Manager(ManagerIn), ClientId(std::move(ClientIdIn)) {}

  RemoteViewportWebRtcSessionManager &Manager;
  std::string ClientId;

  void OnEncodedVideoPacket(const EncodedVideoPacket &Packet) override {
    Manager.HandleClientEncodedVideoPacket(ClientId, Packet);
  }
};

RemoteViewportWebRtcSessionManager::RemoteViewportWebRtcSessionManager(
    RemoteViewportServer &Server)
    : m_Server(Server) {}

size_t RemoteViewportWebRtcSessionManager::GetRemoteClientCount() const {
  std::scoped_lock Lock(m_RemoteClientMutex);
  return m_RemoteClientsById.size();
}

size_t RemoteViewportWebRtcSessionManager::GetActiveWebRtcSessionCount() const {
  std::scoped_lock Lock(m_RemoteClientMutex);
  size_t Count = 0;
  for (const auto &[ClientId, Client] : m_RemoteClientsById) {
    (void)ClientId;
    if (Client->WebRtcSession != nullptr) {
      ++Count;
    }
  }
  return Count;
}

std::vector<std::pair<SessionUserId, std::chrono::steady_clock::time_point>>
RemoteViewportWebRtcSessionManager::CollectPresenceEntries() const {
  std::vector<std::pair<SessionUserId, std::chrono::steady_clock::time_point>>
      Entries;
  std::scoped_lock Lock(m_RemoteClientMutex);
  Entries.reserve(m_RemoteClientsById.size());
  for (const auto &[ClientId, Client] : m_RemoteClientsById) {
    (void)ClientId;
    Entries.emplace_back(Client->User, Client->LastActivity);
  }
  return Entries;
}

std::optional<SessionUserId>
RemoteViewportWebRtcSessionManager::ResolveClientUser(
    std::string_view ClientId) const {
  std::scoped_lock Lock(m_RemoteClientMutex);
  const auto It = m_RemoteClientsById.find(std::string(ClientId));
  if (It == m_RemoteClientsById.end()) {
    return std::nullopt;
  }
  return It->second->User;
}

std::shared_ptr<RemoteClientSession>
RemoteViewportWebRtcSessionManager::FindClientSession(std::string_view ClientId) {
  std::scoped_lock Lock(m_RemoteClientMutex);
  const auto It = m_RemoteClientsById.find(std::string(ClientId));
  return It != m_RemoteClientsById.end() ? It->second : nullptr;
}

std::shared_ptr<const RemoteClientSession>
RemoteViewportWebRtcSessionManager::FindClientSession(
    std::string_view ClientId) const {
  std::scoped_lock Lock(m_RemoteClientMutex);
  const auto It = m_RemoteClientsById.find(std::string(ClientId));
  return It != m_RemoteClientsById.end() ? It->second : nullptr;
}

WebRtcSessionStatus RemoteViewportWebRtcSessionManager::GetClientWebRtcStatus(
    std::string_view ClientId) const {
  const auto Client = FindClientSession(ClientId);
  if (Client == nullptr || Client->WebRtcSession == nullptr) {
    return {};
  }
  return Client->WebRtcSession->GetStatus();
}

std::vector<IWebRtcSession *>
RemoteViewportWebRtcSessionManager::CollectClientWebRtcSessions() const {
  std::vector<IWebRtcSession *> Sessions;
  std::scoped_lock Lock(m_RemoteClientMutex);
  Sessions.reserve(m_RemoteClientsById.size());
  for (const auto &[ClientId, Client] : m_RemoteClientsById) {
    (void)ClientId;
    if (Client->WebRtcSession != nullptr) {
      Sessions.push_back(Client->WebRtcSession.get());
    }
  }
  return Sessions;
}

ClientSessionResolution
RemoteViewportWebRtcSessionManager::CreateOrResumeClientSession(
    const std::optional<std::string> &ClientIdHint) {
  std::shared_ptr<RemoteClientSession> ResolvedSession;
  bool ResumedExisting = false;
  {
    std::scoped_lock Lock(m_RemoteClientMutex);
    if (ClientIdHint.has_value()) {
      const auto Existing = m_RemoteClientsById.find(*ClientIdHint);
      if (Existing != m_RemoteClientsById.end()) {
        Existing->second->LastActivity = std::chrono::steady_clock::now();
        ResolvedSession = Existing->second;
        ResumedExisting = true;
      }
    }
    if (ResolvedSession == nullptr) {
      auto Session = std::make_shared<RemoteClientSession>();
      Session->ClientId = GenerateClientId();
      Session->User = SessionUserId{m_NextRemoteUserId++};
      Session->LastActivity = std::chrono::steady_clock::now();
      Session->WebRtcSession = CreateWebRtcSession();
      Session->VideoEncoder = CreateDefaultVideoEncoder();
      Session->VideoPacketOutput =
          std::make_unique<RemoteClientSession::PacketOutput>(*this,
                                                              Session->ClientId);
      if (Session->VideoEncoder != nullptr &&
          Session->VideoPacketOutput != nullptr) {
        Session->VideoEncoder->SetOutput(Session->VideoPacketOutput.get());
      }
      if (Session->WebRtcSession != nullptr) {
        const std::string ClientId = Session->ClientId;
        Session->WebRtcSession->SetCommandMessageHandler(
            [this, ClientId](std::string_view Payload) {
              m_Server.m_WebSocketDispatch->HandleClientWebRtcMessage(ClientId,
                                                                      Payload);
            });
      }
      m_Server.m_GridSnap->Sanitize(Session->GridSnap);
      m_RemoteClientsById.emplace(Session->ClientId, Session);
      ResolvedSession = std::move(Session);
    }
  }

  m_Server.m_Host.GetSessionModule().GetSession().EnsureViewportState(
      ResolvedSession->User);
  m_Server.m_Host.GetSessionModule().GetSession().SetPresenceState(
      ResolvedSession->User, EditorUserPresenceState::Connected);
  m_Server.m_Host.EnsureRemoteRenderView(ResolvedSession->ClientId,
                                         ResolvedSession->User);
  return {.Session = std::move(ResolvedSession),
          .ResumedExisting = ResumedExisting};
}

void RemoteViewportWebRtcSessionManager::TouchClientSession(
    const std::string &ClientId) {
  {
    std::scoped_lock Lock(m_RemoteClientMutex);
    const auto It = m_RemoteClientsById.find(ClientId);
    if (It != m_RemoteClientsById.end()) {
      It->second->LastActivity = std::chrono::steady_clock::now();
    }
  }
  m_Server.m_Host.FocusRemoteRenderView(ClientId);
}

bool RemoteViewportWebRtcSessionManager::HandleSessionConnectRequest(
    uintptr_t ClientSocketValue, std::string_view HeaderBlock,
    std::string_view Body) {
  (void)Body;
  const auto ClientIdHint = FindHeaderValue(HeaderBlock, ClientIdHeaderName);
  const ClientSessionResolution Resolution =
      CreateOrResumeClientSession(ClientIdHint);
  RemoteClientSession &Client = *Resolution.Session;
  TouchClientSession(Client.ClientId);

  if (Resolution.ResumedExisting && Client.WebRtcSession != nullptr) {
    const WebRtcSessionStatus CurrentStatus = Client.WebRtcSession->GetStatus();
    if (CurrentStatus.ConnectionState != "new" &&
        CurrentStatus.ConnectionState != "closed") {
      Client.WebRtcSession->ResetPeer("client_session_resumed");
    }
  }

  const WebRtcSessionStatus Status =
      Client.WebRtcSession != nullptr ? Client.WebRtcSession->GetStatus()
                                      : WebRtcSessionStatus{};
  const bool ShowColliders = [&]() -> bool {
    if (const HeadlessRenderViewState *View =
            m_Server.m_Host.FindRemoteRenderView(Client.ClientId);
        View != nullptr) {
      return View->ShowColliders;
    }
    return true;
  }();
  const std::string Payload = SerializeSessionConnectResponse(
      Client.ClientId, m_Server.m_Host.GetSessionModule().GetSession().GetState(),
      Client.User, ShowColliders, m_Server.m_TransportConnected.load(),
      m_Server.m_TransportConnected.load() ? "connected" : "disconnected",
      Status.ConnectionState);
  return m_Server.m_HttpRouter->SendJsonResponse(ClientSocketValue, "200 OK",
                                                 Payload);
}

bool RemoteViewportWebRtcSessionManager::HandleWebRtcOfferRequest(
    uintptr_t ClientSocketValue, std::string_view HeaderBlock,
    std::string_view Body) {
  const auto User =
      [&]() -> std::optional<SessionUserId> {
    const auto ClientId = FindHeaderValue(HeaderBlock, ClientIdHeaderName);
    return ClientId.has_value() ? ResolveClientUser(*ClientId) : std::nullopt;
  }();
  const auto ClientId = FindHeaderValue(HeaderBlock, ClientIdHeaderName);
  if (!User.has_value()) {
    return m_Server.m_HttpRouter->SendJsonError(
        ClientSocketValue, "400 Bad Request",
        "Missing or unknown X-Axiom-Client-Id.");
  }
  if (ClientId.has_value()) {
    TouchClientSession(*ClientId);
  }

  std::string Error;
  const auto Offer = ParseWebRtcSessionDescription(Body, Error);
  if (!Offer.has_value()) {
    return m_Server.m_HttpRouter->SendJsonError(ClientSocketValue,
                                                "400 Bad Request", Error);
  }
  if (Offer->Type != "offer") {
    return m_Server.m_HttpRouter->SendJsonError(
        ClientSocketValue, "400 Bad Request",
        "WebRTC offer endpoint requires `type` to be `offer`.");
  }
  if (!ClientId.has_value()) {
    return m_Server.m_HttpRouter->SendJsonError(
        ClientSocketValue, "503 Service Unavailable",
        "Missing X-Axiom-Client-Id for WebRTC session.");
  }

  auto Client = FindClientSession(*ClientId);
  if (Client == nullptr || Client->WebRtcSession == nullptr) {
    return m_Server.m_HttpRouter->SendJsonError(
        ClientSocketValue, "503 Service Unavailable",
        "WebRTC session support is unavailable.");
  }

  WebRtcSessionDescription Answer{};
  if (!Client->WebRtcSession->HandleOffer(*Offer, Answer, Error)) {
    const WebRtcSessionStatus Status = Client->WebRtcSession->GetStatus();
    const std::string Payload = SerializeWebRtcStatus(
        Status.Enabled, Status.Available, Status.SignalingState,
        Status.ConnectionState, Error.empty() ? Status.Detail : Error,
        Status.SessionId, Status.PendingLocalIceCandidateCount, Status.Video);
    return m_Server.m_HttpRouter->SendJsonResponse(
        ClientSocketValue, "503 Service Unavailable", Payload);
  }

  return m_Server.m_HttpRouter->SendJsonResponse(
      ClientSocketValue, "200 OK",
      SerializeWebRtcSessionDescription(
          Answer, Client->WebRtcSession->GetStatus().SessionId));
}

bool RemoteViewportWebRtcSessionManager::HandleWebRtcIceCandidateRequest(
    uintptr_t ClientSocketValue, std::string_view HeaderBlock,
    std::string_view Body) {
  const auto ClientId = FindHeaderValue(HeaderBlock, ClientIdHeaderName);
  const auto User =
      ClientId.has_value() ? ResolveClientUser(*ClientId) : std::nullopt;
  if (!User.has_value()) {
    return m_Server.m_HttpRouter->SendJsonError(
        ClientSocketValue, "400 Bad Request",
        "Missing or unknown X-Axiom-Client-Id.");
  }
  if (ClientId.has_value()) {
    TouchClientSession(*ClientId);
  }

  std::string Error;
  const auto Candidate = ParseWebRtcIceCandidate(Body, Error);
  if (!Candidate.has_value()) {
    return m_Server.m_HttpRouter->SendJsonError(ClientSocketValue,
                                                "400 Bad Request", Error);
  }
  if (!ClientId.has_value()) {
    return m_Server.m_HttpRouter->SendJsonError(
        ClientSocketValue, "503 Service Unavailable",
        "Missing X-Axiom-Client-Id for WebRTC session.");
  }

  auto Client = FindClientSession(*ClientId);
  if (Client == nullptr || Client->WebRtcSession == nullptr) {
    return m_Server.m_HttpRouter->SendJsonError(
        ClientSocketValue, "503 Service Unavailable",
        "WebRTC session support is unavailable.");
  }
  if (!Client->WebRtcSession->AddRemoteIceCandidate(*Candidate, Error)) {
    const WebRtcSessionStatus Status = Client->WebRtcSession->GetStatus();
    const std::string Payload = SerializeWebRtcStatus(
        Status.Enabled, Status.Available, Status.SignalingState,
        Status.ConnectionState, Error.empty() ? Status.Detail : Error,
        Status.SessionId, Status.PendingLocalIceCandidateCount, Status.Video);
    return m_Server.m_HttpRouter->SendJsonResponse(
        ClientSocketValue, "503 Service Unavailable", Payload);
  }

  return m_Server.m_HttpRouter->SendJsonResponse(
      ClientSocketValue, "202 Accepted", "{\"type\":\"accepted\"}");
}

bool RemoteViewportWebRtcSessionManager::HandleWebRtcCloseRequest(
    uintptr_t ClientSocketValue, std::string_view HeaderBlock,
    std::string_view Body) {
  const auto ClientId = FindHeaderValue(HeaderBlock, ClientIdHeaderName);
  const auto User =
      ClientId.has_value() ? ResolveClientUser(*ClientId) : std::nullopt;
  if (!User.has_value()) {
    return m_Server.m_HttpRouter->SendJsonError(
        ClientSocketValue, "400 Bad Request",
        "Missing or unknown X-Axiom-Client-Id.");
  }
  if (ClientId.has_value()) {
    TouchClientSession(*ClientId);
  }

  std::string Reason = Body.empty() ? "browser_requested_close"
                                    : std::string(Body);
  if (!ClientId.has_value()) {
    return m_Server.m_HttpRouter->SendJsonError(
        ClientSocketValue, "503 Service Unavailable",
        "Missing X-Axiom-Client-Id for WebRTC session.");
  }

  auto Client = FindClientSession(*ClientId);
  if (Client == nullptr || Client->WebRtcSession == nullptr) {
    return m_Server.m_HttpRouter->SendJsonError(
        ClientSocketValue, "503 Service Unavailable",
        "WebRTC session support is unavailable.");
  }

  std::string Error;
  if (!Client->WebRtcSession->CloseSession(Reason, Error)) {
    return m_Server.m_HttpRouter->SendJsonError(
        ClientSocketValue, "500 Internal Server Error",
        Error.empty() ? "Failed to close WebRTC session." : Error);
  }

  std::optional<SessionUserId> DisconnectedUser;
  if (const auto Existing = FindClientSession(*ClientId); Existing != nullptr) {
    DisconnectedUser = Existing->User;
  }
  if (DisconnectedUser.has_value()) {
    EditorSession &DisconnectSession = m_Server.m_Host.GetSessionModule().GetSession();
    DisconnectSession.ReleaseAllLocksForUser(*DisconnectedUser);
    DisconnectSession.SetPresenceState(*DisconnectedUser,
                                       EditorUserPresenceState::Disconnected);
  }
  m_Server.m_Host.RemoveRemoteRenderView(*ClientId);
  m_Server.m_Host.FocusLocalRenderView();

  const WebRtcSessionStatus Status = Client->WebRtcSession->GetStatus();
  const std::string Payload = SerializeWebRtcStatus(
      Status.Enabled, Status.Available, Status.SignalingState,
      Status.ConnectionState, Status.Detail, Status.SessionId,
      Status.PendingLocalIceCandidateCount, Status.Video);
  return m_Server.m_HttpRouter->SendJsonResponse(ClientSocketValue, "200 OK",
                                                 Payload);
}

void RemoteViewportWebRtcSessionManager::HandleViewportFrame(
    const ViewportFrame &Frame) {
  if (Frame.User.Value == 0u) {
    return;
  }
  if (const HeadlessRenderViewState *RenderView =
          m_Server.m_Host.FindRenderView(Frame.User);
      RenderView != nullptr && !RenderView->IsLocal) {
    if (auto Client = FindClientSession(RenderView->ClientId); Client != nullptr) {
      if (Client->WebRtcSession != nullptr) {
        Client->WebRtcSession->OnViewportFrame(Frame);
      }
      if (Client->VideoEncoder != nullptr) {
        Client->VideoEncoder->EncodeFrame({
            .FrameIndex = Frame.FrameIndex,
            .Width = Frame.Width,
            .Height = Frame.Height,
            .Format = Frame.Format,
            .Pixels = Frame.Pixels,
        });
      }
    }
  }
}

void RemoteViewportWebRtcSessionManager::HandleClientEncodedVideoPacket(
    std::string_view ClientId, const EncodedVideoPacket &Packet) {
  if (auto Client = FindClientSession(ClientId);
      Client != nullptr && Client->WebRtcSession != nullptr) {
    Client->WebRtcSession->OnEncodedVideoPacket(Packet);
  }
}
} // namespace Axiom
