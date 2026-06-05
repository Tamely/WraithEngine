#include "RemoteViewportServer.h"

#include "HeadlessCommandProtocol.h"
#include "RemoteViewportGizmoController.h"
#include "RemoteViewportGridSnap.h"
#include "RemoteViewportHttpRouter.h"
#include "RemoteViewportPresence.h"
#include "RemoteViewportWebRtcSessionManager.h"
#include "RemoteViewportWebSocketDispatch.h"

#include <App.h>
#include <Loop.h>

#include <charconv>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <string_view>

namespace Axiom {
struct RemoteViewportServerUwsState {
  std::mutex StartupMutex;
  std::condition_variable StartupCondition;
  bool StartupCompleted{false};
  std::string StartupError;
  uWS::Loop *Loop{nullptr};
  std::unique_ptr<uWS::App> App;
  us_listen_socket_t *ListenSocket{nullptr};
};

namespace {
struct RemoteViewportWebSocketUserData {
  uintptr_t ConnectionId{0};
};

using UwsHttpRequest = uWS::HttpRequest;
using UwsHttpResponse = uWS::HttpResponse<false>;
using UwsWebSocket =
    uWS::WebSocket<false, true, RemoteViewportWebSocketUserData>;

std::string BuildHeaderBlock(std::string_view Method, UwsHttpRequest &Request) {
  std::ostringstream Stream;
  Stream << Method << ' ' << Request.getFullUrl() << " HTTP/1.1\r\n";
  for (const auto &[Key, Value] : Request) {
    Stream << Key << ": " << Value << "\r\n";
  }
  Stream << "\r\n";
  return Stream.str();
}
} // namespace

RemoteViewportServer::RemoteViewportServer(
    HeadlessSessionHost &Host, const RemoteViewportServerOptions &Options)
    : m_Host(Host), m_Options(Options) {
  m_GridSnap = std::make_unique<RemoteViewportGridSnap>();
  m_GizmoController = std::make_unique<RemoteViewportGizmoController>(*this);
  m_HttpRouter = std::make_unique<RemoteViewportHttpRouter>(*this);
  m_Presence = std::make_unique<RemoteViewportPresence>(*this);
  m_WebRtcSessions = std::make_unique<RemoteViewportWebRtcSessionManager>(*this);
  m_WebSocketDispatch =
      std::make_unique<RemoteViewportWebSocketDispatch>(*this);
  m_Host.SetTransportVideoEncoder(nullptr);
}

RemoteViewportServer::~RemoteViewportServer() { Stop(); }

RemoteViewportServerMetrics RemoteViewportServer::GetMetrics() const {
  RemoteViewportServerMetrics Metrics{};
  Metrics.TransportConnected = m_TransportConnected.load();
  Metrics.ListenPort = m_Options.Port;
  Metrics.ActiveWebSocketClients = m_WebSocketDispatch->GetActiveClientCount();
  Metrics.ActiveRemoteClients = m_WebRtcSessions->GetRemoteClientCount();
  Metrics.ActiveWebRtcSessions = m_WebRtcSessions->GetActiveWebRtcSessionCount();
  Metrics.TotalHttpRequests = m_HttpRouter->GetTotalHttpRequests();
  Metrics.TotalWebSocketMessages =
      m_WebSocketDispatch->GetTotalWebSocketMessages();
  return Metrics;
}

bool RemoteViewportServer::Start(std::string &Error) {
  m_StopRequested.store(false);
  m_UwsState = std::make_unique<RemoteViewportServerUwsState>();
  m_Host.GetTransport().Connect(this);
  m_ServerThread = std::thread([this]() {
    RemoteViewportServerUwsState *State = m_UwsState.get();
    if (State == nullptr) {
      return;
    }

    State->Loop = uWS::Loop::get();
    State->App = std::make_unique<uWS::App>();

    auto RegisterGetHandler = [this](UwsHttpResponse *Response,
                                     UwsHttpRequest *Request) {
      const uintptr_t ConnectionId = m_HttpRouter->AllocateConnectionId();
      m_HttpRouter->RegisterPendingResponse(ConnectionId, Response);
      Response->onAborted([this, ConnectionId]() {
        m_HttpRouter->MarkPendingResponseAborted(ConnectionId);
      });

      const std::string HeaderBlock = BuildHeaderBlock("GET", *Request);
      m_HttpRouter->IncrementRequestCount();
      m_HttpRouter->HandleGetRequest(ConnectionId,
                                     std::string(Request->getFullUrl()),
                                     HeaderBlock);
    };

    auto RegisterOptionsHandler = [this](UwsHttpResponse *Response,
                                         UwsHttpRequest *Request) {
      (void)Request;
      const uintptr_t ConnectionId = m_HttpRouter->AllocateConnectionId();
      m_HttpRouter->RegisterPendingResponse(ConnectionId, Response);
      Response->onAborted([this, ConnectionId]() {
        m_HttpRouter->MarkPendingResponseAborted(ConnectionId);
      });

      m_HttpRouter->IncrementRequestCount();
      m_HttpRouter->SendHttpResponse(
          ConnectionId,
          "HTTP/1.1 204 No Content\r\n"
          "Content-Type: text/plain; charset=utf-8\r\n"
          "Content-Length: 0\r\n"
          "Cache-Control: no-store\r\n"
          "Connection: close\r\n"
          "Access-Control-Allow-Origin: *\r\n"
          "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
          "Access-Control-Allow-Headers: Content-Type, X-Axiom-Client-Id\r\n"
          "\r\n");
    };

    auto RegisterPostHandler = [this](UwsHttpResponse *Response,
                                      UwsHttpRequest *Request) {
      const uintptr_t ConnectionId = m_HttpRouter->AllocateConnectionId();
      m_HttpRouter->RegisterPendingResponse(ConnectionId, Response);
      Response->onAborted([this, ConnectionId]() {
        m_HttpRouter->MarkPendingResponseAborted(ConnectionId);
      });

      auto HeaderBlock =
          std::make_shared<std::string>(BuildHeaderBlock("POST", *Request));
      auto Path = std::make_shared<std::string>(Request->getFullUrl());
      auto Body = std::make_shared<std::string>();
      m_HttpRouter->IncrementRequestCount();

      const std::string_view ContentLength = Request->getHeader("content-length");
      if (ContentLength.empty() || ContentLength == "0") {
        m_HttpRouter->HandlePostRequest(ConnectionId, *Path, *HeaderBlock, "");
        return;
      }

      Response->onData([this, ConnectionId, HeaderBlock, Path, Body](
                           std::string_view Chunk, bool IsLast) {
        Body->append(Chunk.data(), Chunk.size());
        if (IsLast) {
          m_HttpRouter->HandlePostRequest(ConnectionId, *Path, *HeaderBlock,
                                          *Body);
        }
      });
    };

    uWS::App::WebSocketBehavior<RemoteViewportWebSocketUserData> Behavior{};
    Behavior.compression = uWS::DISABLED;
    Behavior.maxPayloadLength = 256 * 1024;
    Behavior.upgrade =
        [this](UwsHttpResponse *Response, UwsHttpRequest *Request,
               us_socket_context_t *Context) {
          const uintptr_t ConnectionId = m_HttpRouter->AllocateConnectionId();
          Response->template upgrade<RemoteViewportWebSocketUserData>(
              {.ConnectionId = ConnectionId},
              Request->getHeader("sec-websocket-key"),
              Request->getHeader("sec-websocket-protocol"),
              Request->getHeader("sec-websocket-extensions"), Context);
        };
    Behavior.open = [this](UwsWebSocket *Socket) {
      const uintptr_t ConnectionId = Socket->getUserData()->ConnectionId;
      m_WebSocketDispatch->OnClientOpen(ConnectionId, Socket);
    };
    Behavior.message = [this](UwsWebSocket *Socket, std::string_view Message,
                              uWS::OpCode OpCode) {
      if (OpCode != uWS::OpCode::TEXT) {
        return;
      }
      const uintptr_t ConnectionId = Socket->getUserData()->ConnectionId;
      if (!m_WebSocketDispatch->HandleWebSocketMessage(ConnectionId, Message)) {
        m_WebSocketDispatch->SendTextMessage(
            ConnectionId,
            SerializeError("Invalid WebSocket command payload."));
      }
    };
    Behavior.close = [this](UwsWebSocket *Socket, int Code,
                            std::string_view Message) {
      (void)Code;
      (void)Message;
      m_WebSocketDispatch->OnClientClose(Socket->getUserData()->ConnectionId);
    };

    State->App->ws<RemoteViewportWebSocketUserData>("/ws", std::move(Behavior))
        .get("/*", std::move(RegisterGetHandler))
        .post("/*", std::move(RegisterPostHandler))
        .options("/*", std::move(RegisterOptionsHandler))
        .listen(m_Options.Host, static_cast<int>(m_Options.Port),
                [State](us_listen_socket_t *ListenSocket) {
                  std::scoped_lock Lock(State->StartupMutex);
                  State->ListenSocket = ListenSocket;
                  State->StartupCompleted = true;
                  if (ListenSocket == nullptr) {
                    State->StartupError =
                        "Failed to bind the remote viewport server socket.";
                  }
                  State->StartupCondition.notify_all();
                });

    {
      std::scoped_lock Lock(State->StartupMutex);
      if (!State->StartupCompleted) {
        State->StartupCompleted = true;
        State->StartupError =
            "uWebSockets did not complete remote viewport startup.";
        State->StartupCondition.notify_all();
      }
    }

    if (State->ListenSocket != nullptr) {
      State->Loop->run();
    }

    State->App.reset();
    if (State->Loop != nullptr) {
      State->Loop->free();
      State->Loop = nullptr;
    }
  });

  {
    std::unique_lock Lock(m_UwsState->StartupMutex);
    m_UwsState->StartupCondition.wait(
        Lock, [this]() { return m_UwsState->StartupCompleted; });
    Error = m_UwsState->StartupError;
  }

  if (!Error.empty()) {
    m_StopRequested.store(true);
    if (m_ServerThread.joinable()) {
      m_ServerThread.join();
    }
    m_Host.GetTransport().Disconnect(this);
    m_UwsState.reset();
    return false;
  }

  m_PresenceThread = std::thread([this]() { m_Presence->RunLoop(); });
  return true;
}

void RemoteViewportServer::Stop() {
  const bool WasStopping = m_StopRequested.exchange(true);
  if (WasStopping) {
    return;
  }

  m_WebSocketDispatch->CloseAllClients();
  if (m_UwsState != nullptr && m_UwsState->Loop != nullptr) {
    RemoteViewportServerUwsState *State = m_UwsState.get();
    m_UwsState->Loop->defer([State]() {
      if (State->ListenSocket != nullptr) {
        us_listen_socket_close(0, State->ListenSocket);
        State->ListenSocket = nullptr;
      }
      if (State->App != nullptr) {
        State->App->close();
      }
    });
  }
  if (m_ServerThread.joinable()) {
    m_ServerThread.join();
  }
  if (m_PresenceThread.joinable()) {
    m_PresenceThread.join();
  }

  m_Host.GetTransport().Disconnect(this);
  for (IWebRtcSession *Session : m_WebRtcSessions->CollectClientWebRtcSessions()) {
    Session->ResetPeer("server_stopped");
  }
  m_UwsState.reset();
}

void RemoteViewportServer::OnSessionTransportConnected() {
  m_TransportConnected.store(true);
  std::cout << SerializeConnected() << std::endl;
}

void RemoteViewportServer::OnSessionTransportDisconnected() {
  m_TransportConnected.store(false);
  std::cout << SerializeDisconnected() << std::endl;
}

void RemoteViewportServer::OnSessionTransportEditorEvent(
    const PublishedEditorEvent &Event) {
  const std::string Message = SerializeEvent(Event);
  m_WebSocketDispatch->BroadcastTextMessage(Message);
  for (IWebRtcSession *Session : m_WebRtcSessions->CollectClientWebRtcSessions()) {
    Session->SendReliableMessage(Message);
  }
}

void RemoteViewportServer::OnSessionTransportViewportFrame(
    const ViewportFrame &Frame) {
  m_WebRtcSessions->HandleViewportFrame(Frame);
}

bool ParseRemoteViewportServerOptions(int argc, char **argv,
                                      RemoteViewportServerOptions &Options,
                                      std::string &Error) {
  for (int Index = 1; Index < argc; ++Index) {
    const std::string_view Argument(argv[Index]);
    if (Argument == "--port" && Index + 1 < argc) {
      uint16_t Port = 0;
      const std::string_view Value(argv[++Index]);
      const auto [Ptr, Ec] =
          std::from_chars(Value.data(), Value.data() + Value.size(), Port);
      if (Ec != std::errc{} || Ptr != Value.data() + Value.size() || Port == 0) {
        Error = "Invalid --port value.";
        return false;
      }
      Options.Port = Port;
    } else if (Argument == "--host" && Index + 1 < argc) {
      Options.Host = argv[++Index];
    } else if (Argument == "--width" && Index + 1 < argc) {
      uint32_t Width = 0;
      const std::string_view Value(argv[++Index]);
      const auto [Ptr, Ec] =
          std::from_chars(Value.data(), Value.data() + Value.size(), Width);
      if (Ec != std::errc{} || Ptr != Value.data() + Value.size() || Width == 0) {
        Error = "Invalid --width value.";
        return false;
      }
      Options.Width = Width;
    } else if (Argument == "--height" && Index + 1 < argc) {
      uint32_t Height = 0;
      const std::string_view Value(argv[++Index]);
      const auto [Ptr, Ec] =
          std::from_chars(Value.data(), Value.data() + Value.size(), Height);
      if (Ec != std::errc{} || Ptr != Value.data() + Value.size() ||
          Height == 0) {
        Error = "Invalid --height value.";
        return false;
      }
      Options.Height = Height;
    } else {
      Error = "Unknown or incomplete argument: " + std::string(Argument);
      return false;
    }
  }

  return true;
}
} // namespace Axiom
