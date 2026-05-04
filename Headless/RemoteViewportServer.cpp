#include "RemoteViewportServer.h"

#include "HeadlessCommandProtocol.h"
#include "RemoteViewportBrowserAssets.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cctype>
#include <cstring>
#include <iostream>
#include <optional>
#include <sstream>
#include <string_view>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace Axiom {
namespace {
#ifdef _WIN32
constexpr SOCKET InvalidSocket = INVALID_SOCKET;

class WinsockRuntime final {
public:
  WinsockRuntime() {
    static std::once_flag Flag;
    std::call_once(Flag, []() {
      WSADATA Data{};
      WSAStartup(MAKEWORD(2, 2), &Data);
    });
  }
};

SOCKET ToSocket(uintptr_t Value) { return static_cast<SOCKET>(Value); }
uintptr_t ToValue(SOCKET Socket) { return static_cast<uintptr_t>(Socket); }

void CloseSocket(SOCKET Socket) {
  if (Socket != InvalidSocket) {
    closesocket(Socket);
  }
}
#endif

struct HttpRequest {
  std::string Method;
  std::string Path;
  std::string Body;
  size_t ContentLength{0};
};

struct PngBuffer {
  std::vector<unsigned char> Bytes;
};

void WritePngToVector(void *Context, void *Data, int Size) {
  auto *Buffer = static_cast<PngBuffer *>(Context);
  const auto *Bytes = static_cast<const unsigned char *>(Data);
  Buffer->Bytes.insert(Buffer->Bytes.end(), Bytes, Bytes + Size);
}

std::optional<CapturedFrame> ConvertFrame(const ViewportFrame &Frame) {
  if (Frame.Format != ViewportFrameFormat::R8G8B8A8Unorm) {
    return std::nullopt;
  }

  CapturedFrame Captured{};
  Captured.FrameIndex = Frame.FrameIndex;
  Captured.Width = Frame.Width;
  Captured.Height = Frame.Height;
  Captured.Pixels.resize(Frame.Pixels.size());
  std::memcpy(Captured.Pixels.data(), Frame.Pixels.data(), Frame.Pixels.size());
  return Captured;
}

std::string BuildHttpResponse(std::string_view Status, std::string_view ContentType,
                              std::string_view Body,
                              std::string_view ExtraHeaders = {}) {
  std::ostringstream Stream;
  Stream << "HTTP/1.1 " << Status << "\r\n"
         << "Content-Type: " << ContentType << "\r\n"
         << "Content-Length: " << Body.size() << "\r\n"
         << "Cache-Control: no-store\r\n"
         << "Connection: close\r\n"
         << "Access-Control-Allow-Origin: *\r\n";
  if (!ExtraHeaders.empty()) {
    Stream << ExtraHeaders;
  }
  Stream << "\r\n" << Body;
  return Stream.str();
}

std::string BuildBinaryResponse(std::string_view Status,
                                std::string_view ContentType, size_t Size,
                                std::string_view ExtraHeaders = {}) {
  std::ostringstream Stream;
  Stream << "HTTP/1.1 " << Status << "\r\n"
         << "Content-Type: " << ContentType << "\r\n"
         << "Content-Length: " << Size << "\r\n"
         << "Cache-Control: no-store\r\n"
         << "Connection: close\r\n"
         << "Access-Control-Allow-Origin: *\r\n";
  if (!ExtraHeaders.empty()) {
    Stream << ExtraHeaders;
  }
  Stream << "\r\n";
  return Stream.str();
}

bool SendAll(SOCKET Socket, const void *Data, size_t Size) {
  const auto *Bytes = static_cast<const char *>(Data);
  size_t Offset = 0;
  while (Offset < Size) {
    const int Sent =
        send(Socket, Bytes + Offset, static_cast<int>(Size - Offset), 0);
    if (Sent == SOCKET_ERROR || Sent == 0) {
      return false;
    }
    Offset += static_cast<size_t>(Sent);
  }
  return true;
}

std::string_view StripQuery(std::string_view Path) {
  const size_t Query = Path.find('?');
  return Query == std::string_view::npos ? Path : Path.substr(0, Query);
}

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

std::optional<size_t> ParseContentLength(std::string_view Headers) {
  const std::string_view Key = "Content-Length:";
  size_t Position = Headers.find(Key);
  if (Position == std::string_view::npos) {
    return 0u;
  }

  Position += Key.size();
  const size_t End = Headers.find("\r\n", Position);
  const std::string Value =
      Trim(Headers.substr(Position, End == std::string_view::npos
                                        ? std::string_view::npos
                                        : End - Position));
  size_t Result = 0;
  const auto [Ptr, Ec] =
      std::from_chars(Value.data(), Value.data() + Value.size(), Result);
  if (Ec != std::errc{} || Ptr != Value.data() + Value.size()) {
    return std::nullopt;
  }
  return Result;
}

bool ReadHttpRequest(SOCKET Socket, HttpRequest &Request, std::string &Error) {
  std::string Buffer;
  std::array<char, 4096> Chunk{};
  size_t HeaderEnd = std::string::npos;

  while (HeaderEnd == std::string::npos) {
    const int Received = recv(Socket, Chunk.data(), static_cast<int>(Chunk.size()), 0);
    if (Received <= 0) {
      Error = "Failed to read HTTP request headers.";
      return false;
    }
    Buffer.append(Chunk.data(), static_cast<size_t>(Received));
    HeaderEnd = Buffer.find("\r\n\r\n");
  }

  const std::string_view HeaderView(Buffer.data(), HeaderEnd + 4);
  const size_t RequestLineEnd = HeaderView.find("\r\n");
  if (RequestLineEnd == std::string_view::npos) {
    Error = "Malformed HTTP request line.";
    return false;
  }

  const std::string_view RequestLine = HeaderView.substr(0, RequestLineEnd);
  const size_t MethodEnd = RequestLine.find(' ');
  const size_t PathEnd =
      MethodEnd == std::string_view::npos ? std::string_view::npos
                                          : RequestLine.find(' ', MethodEnd + 1);
  if (MethodEnd == std::string_view::npos || PathEnd == std::string_view::npos) {
    Error = "Malformed HTTP request line.";
    return false;
  }

  Request.Method = std::string(RequestLine.substr(0, MethodEnd));
  Request.Path =
      std::string(RequestLine.substr(MethodEnd + 1, PathEnd - MethodEnd - 1));

  const auto ContentLength = ParseContentLength(HeaderView);
  if (!ContentLength.has_value()) {
    Error = "Invalid Content-Length header.";
    return false;
  }
  Request.ContentLength = *ContentLength;

  const size_t BodyOffset = HeaderEnd + 4;
  while (Buffer.size() < BodyOffset + Request.ContentLength) {
    const int Received = recv(Socket, Chunk.data(), static_cast<int>(Chunk.size()), 0);
    if (Received <= 0) {
      Error = "Failed to read HTTP request body.";
      return false;
    }
    Buffer.append(Chunk.data(), static_cast<size_t>(Received));
  }

  Request.Body.assign(Buffer.data() + BodyOffset, Request.ContentLength);
  return true;
}

std::string JsonResponse(std::string_view Status, std::string_view Payload) {
  return BuildHttpResponse(Status, "application/json; charset=utf-8", Payload);
}
} // namespace

RemoteViewportServer::RemoteViewportServer(
    HeadlessSessionHost &Host, const RemoteViewportServerOptions &Options)
    : m_Host(Host), m_Options(Options) {}

RemoteViewportServer::~RemoteViewportServer() { Stop(); }

bool RemoteViewportServer::Start(std::string &Error) {
#ifdef _WIN32
  WinsockRuntime Winsock;
  (void)Winsock;

  addrinfo Hint{};
  Hint.ai_family = AF_INET;
  Hint.ai_socktype = SOCK_STREAM;
  Hint.ai_protocol = IPPROTO_TCP;
  Hint.ai_flags = AI_PASSIVE;

  addrinfo *AddressInfo = nullptr;
  const std::string PortString = std::to_string(m_Options.Port);
  if (getaddrinfo(m_Options.Host.c_str(), PortString.c_str(), &Hint,
                  &AddressInfo) != 0) {
    Error = "Failed to resolve the remote viewport server address.";
    return false;
  }

  SOCKET ListenSocket = InvalidSocket;
  for (addrinfo *Current = AddressInfo; Current != nullptr;
       Current = Current->ai_next) {
    ListenSocket =
        socket(Current->ai_family, Current->ai_socktype, Current->ai_protocol);
    if (ListenSocket == InvalidSocket) {
      continue;
    }

    constexpr int Reuse = 1;
    setsockopt(ListenSocket, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char *>(&Reuse), sizeof(Reuse));

    if (bind(ListenSocket, Current->ai_addr, static_cast<int>(Current->ai_addrlen)) ==
        0 &&
        listen(ListenSocket, SOMAXCONN) == 0) {
      break;
    }

    CloseSocket(ListenSocket);
    ListenSocket = InvalidSocket;
  }

  freeaddrinfo(AddressInfo);

  if (ListenSocket == InvalidSocket) {
    Error = "Failed to bind the remote viewport server socket.";
    return false;
  }

  m_ListenSocket = ToValue(ListenSocket);
  m_StopRequested.store(false);
  m_Host.GetTransport().Connect(this);
  m_AcceptThread = std::thread([this]() { AcceptLoop(); });
  return true;
#else
  Error = "Remote viewport server is currently implemented for Windows only.";
  return false;
#endif
}

void RemoteViewportServer::Stop() {
  const bool WasStopping = m_StopRequested.exchange(true);
  if (WasStopping) {
    return;
  }

#ifdef _WIN32
  const SOCKET ListenSocket = ToSocket(m_ListenSocket);
  m_ListenSocket = ToValue(InvalidSocket);
  CloseSocket(ListenSocket);
  CloseAllClients();
  if (m_AcceptThread.joinable()) {
    m_AcceptThread.join();
  }
#endif

  m_Host.GetTransport().Disconnect(this);
}

void RemoteViewportServer::OnSessionTransportConnected() {
  std::cout << SerializeConnected() << std::endl;
}

void RemoteViewportServer::OnSessionTransportDisconnected() {
  std::cout << SerializeDisconnected() << std::endl;
}

void RemoteViewportServer::OnSessionTransportEditorEvent(
    const PublishedEditorEvent &Event) {
  BroadcastSse(SerializeEvent(Event));
}

void RemoteViewportServer::OnSessionTransportViewportFrame(
    const ViewportFrame &Frame) {
  const auto Captured = ConvertFrame(Frame);
  if (!Captured.has_value()) {
    BroadcastSse(SerializeError("Unsupported viewport frame format."));
    return;
  }

  SetLatestFrame(*Captured);
  BroadcastSse(SerializeFrameMetadata(Captured->FrameIndex, Captured->Width,
                                      Captured->Height));
}

void RemoteViewportServer::AcceptLoop() {
#ifdef _WIN32
  const SOCKET ListenSocket = ToSocket(m_ListenSocket);
  while (!m_StopRequested.load()) {
    sockaddr_in ClientAddress{};
    int ClientAddressLength = sizeof(ClientAddress);
    SOCKET ClientSocket =
        accept(ListenSocket, reinterpret_cast<sockaddr *>(&ClientAddress),
               &ClientAddressLength);
    if (ClientSocket == InvalidSocket) {
      if (!m_StopRequested.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
      }
      continue;
    }

    std::thread(&RemoteViewportServer::HandleClient, this, ToValue(ClientSocket))
        .detach();
  }
#endif
}

void RemoteViewportServer::HandleClient(uintptr_t ClientSocketValue) {
#ifdef _WIN32
  const SOCKET ClientSocket = ToSocket(ClientSocketValue);
  if (HandleHttpRequest(ClientSocketValue)) {
    return;
  }
  CloseSocket(ClientSocket);
#else
  (void)ClientSocketValue;
#endif
}

void RemoteViewportServer::BroadcastSse(std::string Message) {
  std::vector<uintptr_t> FailedClients;
  std::vector<uintptr_t> Clients;
  {
    std::scoped_lock Lock(m_ClientMutex);
    Clients = m_SseClients;
  }

  for (const uintptr_t Client : Clients) {
    if (!SendSseMessage(Client, Message)) {
      FailedClients.push_back(Client);
    }
  }

  for (const uintptr_t Client : FailedClients) {
    RemoveClient(Client);
  }
}

void RemoteViewportServer::CloseAllClients() {
#ifdef _WIN32
  std::vector<uintptr_t> Clients;
  {
    std::scoped_lock Lock(m_ClientMutex);
    Clients.swap(m_SseClients);
  }

  for (const uintptr_t Client : Clients) {
    CloseSocket(ToSocket(Client));
  }
#endif
}

void RemoteViewportServer::RemoveClient(uintptr_t ClientSocketValue) {
#ifdef _WIN32
  bool Removed = false;
  {
    std::scoped_lock Lock(m_ClientMutex);
    auto It = std::find(m_SseClients.begin(), m_SseClients.end(), ClientSocketValue);
    if (It != m_SseClients.end()) {
      m_SseClients.erase(It);
      Removed = true;
    }
  }

  if (Removed) {
    CloseSocket(ToSocket(ClientSocketValue));
    std::cout << SerializeDisconnected() << std::endl;
  }
#else
  (void)ClientSocketValue;
#endif
}

bool RemoteViewportServer::SendSseMessage(uintptr_t ClientSocketValue,
                                          std::string_view Message) {
#ifdef _WIN32
  std::string Payload = "data: " + std::string(Message) + "\n\n";
  return SendAll(ToSocket(ClientSocketValue), Payload.data(), Payload.size());
#else
  (void)ClientSocketValue;
  (void)Message;
  return false;
#endif
}

bool RemoteViewportServer::HandleHttpRequest(uintptr_t ClientSocketValue) {
#ifdef _WIN32
  const SOCKET ClientSocket = ToSocket(ClientSocketValue);
  HttpRequest Request{};
  std::string Error;
  if (!ReadHttpRequest(ClientSocket, Request, Error)) {
    const std::string Response =
        JsonResponse("400 Bad Request", SerializeError(Error));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  if (Request.Method == "GET") {
    return HandleGetRequest(ClientSocketValue, Request.Path);
  }
  if (Request.Method == "POST") {
    return HandlePostRequest(ClientSocketValue, Request.Path, Request.Body);
  }
  if (Request.Method == "OPTIONS") {
    const std::string Response = BuildHttpResponse(
        "204 No Content", "text/plain; charset=utf-8", "",
        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n");
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  const std::string Response = JsonResponse(
      "405 Method Not Allowed",
      SerializeError("Only GET, POST, and OPTIONS are supported."));
  SendAll(ClientSocket, Response.data(), Response.size());
  return false;
#else
  (void)ClientSocketValue;
  return false;
#endif
}

bool RemoteViewportServer::HandleGetRequest(uintptr_t ClientSocketValue,
                                            std::string_view Path) {
#ifdef _WIN32
  const SOCKET ClientSocket = ToSocket(ClientSocketValue);
  const std::string_view Route = StripQuery(Path);
  RemoteViewportBrowserAsset BrowserAsset{};
  if (TryGetRemoteViewportBrowserAsset(Route, BrowserAsset)) {
    const std::string Body(BrowserAsset.Body);
    const std::string Response =
        BuildHttpResponse("200 OK", BrowserAsset.ContentType, Body);
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }
  if (Route == "/health") {
    const std::string Body =
        SerializeReady(m_Options.Width, m_Options.Height);
    const std::string Response = JsonResponse("200 OK", Body);
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }
  if (Route == "/frame") {
    LatestFrame Frame{};
    if (!TryGetLatestFrame(Frame)) {
      const std::string Response = JsonResponse(
          "404 Not Found", SerializeError("No captured frame is available yet."));
      SendAll(ClientSocket, Response.data(), Response.size());
      return false;
    }

    const std::string Headers =
        BuildBinaryResponse("200 OK", "image/png", Frame.PngBytes.size());
    if (!SendAll(ClientSocket, Headers.data(), Headers.size())) {
      return false;
    }
    SendAll(ClientSocket, Frame.PngBytes.data(), Frame.PngBytes.size());
    return false;
  }
  if (Route == "/events") {
    std::ostringstream Response;
    Response << "HTTP/1.1 200 OK\r\n"
             << "Content-Type: text/event-stream\r\n"
             << "Cache-Control: no-cache\r\n"
             << "Connection: keep-alive\r\n"
             << "Access-Control-Allow-Origin: *\r\n\r\n";
    const std::string ResponseText = Response.str();
    if (!SendAll(ClientSocket, ResponseText.data(), ResponseText.size())) {
      return false;
    }

    {
      std::scoped_lock Lock(m_ClientMutex);
      m_SseClients.push_back(ClientSocketValue);
    }

    std::cout << SerializeConnected() << std::endl;
    SendSseMessage(ClientSocketValue,
                   SerializeReady(m_Options.Width, m_Options.Height));
    SendSseMessage(ClientSocketValue, SerializeConnected());

    LatestFrame Frame{};
    if (TryGetLatestFrame(Frame)) {
      SendSseMessage(ClientSocketValue,
                     SerializeFrameMetadata(Frame.FrameIndex, Frame.Width,
                                            Frame.Height));
    }
    return true;
  }

  const std::string Response =
      JsonResponse("404 Not Found", SerializeError("Unknown GET endpoint."));
  SendAll(ClientSocket, Response.data(), Response.size());
  return false;
#else
  (void)ClientSocketValue;
  (void)Path;
  return false;
#endif
}

bool RemoteViewportServer::HandlePostRequest(uintptr_t ClientSocketValue,
                                             std::string_view Path,
                                             std::string_view Body) {
#ifdef _WIN32
  const SOCKET ClientSocket = ToSocket(ClientSocketValue);
  const std::string_view Route = StripQuery(Path);
  if (Route != "/command") {
    const std::string Response =
        JsonResponse("404 Not Found", SerializeError("Unknown POST endpoint."));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  std::string Error;
  const auto Command = ParseRemoteViewportCommand(Body, Error);
  if (!Command.has_value()) {
    const std::string Response =
        JsonResponse("400 Bad Request", SerializeError(Error));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  switch (Command->Type) {
  case HeadlessCommandType::SetViewMode:
    m_Host.SetRemoteViewMode(Command->ViewMode);
    break;
  case HeadlessCommandType::SetLookActive:
  case HeadlessCommandType::UpdateViewportCamera:
    m_Host.SubmitRemoteCommand(Command->EditorPayload);
    break;
  case HeadlessCommandType::Quit:
    m_StopRequested.store(true);
    m_Host.RequestClose();
    BroadcastSse(SerializeShutdown());
    break;
  case HeadlessCommandType::LoadStartupScene:
  case HeadlessCommandType::RenderFrame:
    break;
  }

  const std::string Response =
      JsonResponse("202 Accepted", "{\"type\":\"accepted\"}");
  SendAll(ClientSocket, Response.data(), Response.size());
  return false;
#else
  (void)ClientSocketValue;
  (void)Path;
  (void)Body;
  return false;
#endif
}

void RemoteViewportServer::SetLatestFrame(const CapturedFrame &Frame) {
  PngBuffer Buffer{};
  if (stbi_write_png_to_func(WritePngToVector, &Buffer,
                             static_cast<int>(Frame.Width),
                             static_cast<int>(Frame.Height), 4,
                             Frame.Pixels.data(),
                             static_cast<int>(Frame.Width * 4u)) == 0) {
    BroadcastSse(SerializeError("Failed to encode a PNG frame."));
    return;
  }

  std::scoped_lock Lock(m_FrameMutex);
  m_LatestFrame.FrameIndex = Frame.FrameIndex;
  m_LatestFrame.Width = Frame.Width;
  m_LatestFrame.Height = Frame.Height;
  m_LatestFrame.PngBytes = std::move(Buffer.Bytes);
}

bool RemoteViewportServer::TryGetLatestFrame(LatestFrame &Frame) const {
  std::scoped_lock Lock(m_FrameMutex);
  if (m_LatestFrame.PngBytes.empty()) {
    return false;
  }
  Frame = m_LatestFrame;
  return true;
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
