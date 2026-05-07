#include "RemoteViewportServer.h"

#include <Core/Platform.h>

#include "HeadlessCommandProtocol.h"
#include <Renderer/VideoEncoderFactory.h>
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

#if AXIOM_PLATFORM_WINDOWS
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace Axiom {
namespace {
constexpr std::string_view WebSocketGuid =
    "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
constexpr std::string_view ClientIdHeaderName = "X-Axiom-Client-Id";

#if AXIOM_PLATFORM_WINDOWS
using SocketHandle = SOCKET;
constexpr SocketHandle InvalidSocket = INVALID_SOCKET;

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
#else
using SocketHandle = int;
constexpr SocketHandle InvalidSocket = -1;
#endif

SocketHandle ToSocket(uintptr_t Value) {
  return static_cast<SocketHandle>(Value);
}

uintptr_t ToValue(SocketHandle Socket) { return static_cast<uintptr_t>(Socket); }

void CloseSocket(SocketHandle Socket) {
  if (Socket != InvalidSocket) {
#if AXIOM_PLATFORM_WINDOWS
    closesocket(Socket);
#else
    close(Socket);
#endif
  }
}

void SetReuseAddress(SocketHandle Socket) {
  constexpr int Reuse = 1;
#if AXIOM_PLATFORM_WINDOWS
  setsockopt(Socket, SOL_SOCKET, SO_REUSEADDR,
             reinterpret_cast<const char *>(&Reuse), sizeof(Reuse));
#else
  setsockopt(Socket, SOL_SOCKET, SO_REUSEADDR, &Reuse, sizeof(Reuse));
#endif
}

struct HttpRequest {
  std::string Method;
  std::string Path;
  std::string HeaderBlock;
  std::string Body;
  size_t ContentLength{0};
};

struct EncodedImageBuffer {
  std::vector<unsigned char> Bytes;
};

void WriteImageBytes(void *Context, void *Data, int Size) {
  auto *Buffer = static_cast<EncodedImageBuffer *>(Context);
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

std::string BuildHttpResponse(std::string_view Status,
                              std::string_view ContentType,
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

bool SendAll(SocketHandle Socket, const void *Data, size_t Size) {
  const auto *Bytes = static_cast<const char *>(Data);
  size_t Offset = 0;
  while (Offset < Size) {
#if AXIOM_PLATFORM_WINDOWS
    const int Sent =
        send(Socket, Bytes + Offset, static_cast<int>(Size - Offset), 0);
    if (Sent == SOCKET_ERROR || Sent == 0) {
#else
    const ssize_t Sent = send(Socket, Bytes + Offset, Size - Offset, 0);
    if (Sent <= 0) {
#endif
      return false;
    }
    Offset += static_cast<size_t>(Sent);
  }
  return true;
}

bool RecvExact(SocketHandle Socket, void *Data, size_t Size) {
  auto *Bytes = static_cast<unsigned char *>(Data);
  size_t Offset = 0;
  while (Offset < Size) {
#if AXIOM_PLATFORM_WINDOWS
    const int Received =
        recv(Socket, reinterpret_cast<char *>(Bytes + Offset),
             static_cast<int>(Size - Offset), 0);
#else
    const ssize_t Received = recv(
        Socket, reinterpret_cast<char *>(Bytes + Offset), Size - Offset, 0);
#endif
    if (Received <= 0) {
      return false;
    }
    Offset += static_cast<size_t>(Received);
  }
  return true;
}

bool RecvChunk(SocketHandle Socket, char *Data, size_t Size, size_t &ReceivedOut) {
#if AXIOM_PLATFORM_WINDOWS
  const int Received = recv(Socket, Data, static_cast<int>(Size), 0);
#else
  const ssize_t Received = recv(Socket, Data, Size, 0);
#endif
  if (Received <= 0) {
    ReceivedOut = 0;
    return false;
  }

  ReceivedOut = static_cast<size_t>(Received);
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

bool ReadHttpRequest(SocketHandle Socket, HttpRequest &Request,
                     std::string &Error) {
  std::string Buffer;
  std::array<char, 4096> Chunk{};
  size_t HeaderEnd = std::string::npos;

  while (HeaderEnd == std::string::npos) {
    size_t Received = 0;
    if (!RecvChunk(Socket, Chunk.data(), Chunk.size(), Received)) {
      Error = "Failed to read HTTP request headers.";
      return false;
    }
    Buffer.append(Chunk.data(), Received);
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
  Request.HeaderBlock = std::string(HeaderView);

  const auto ContentLength = ParseContentLength(HeaderView);
  if (!ContentLength.has_value()) {
    Error = "Invalid Content-Length header.";
    return false;
  }
  Request.ContentLength = *ContentLength;

  const size_t BodyOffset = HeaderEnd + 4;
  while (Buffer.size() < BodyOffset + Request.ContentLength) {
    size_t Received = 0;
    if (!RecvChunk(Socket, Chunk.data(), Chunk.size(), Received)) {
      Error = "Failed to read HTTP request body.";
      return false;
    }
    Buffer.append(Chunk.data(), Received);
  }

  Request.Body.assign(Buffer.data() + BodyOffset, Request.ContentLength);
  return true;
}

std::string JsonResponse(std::string_view Status, std::string_view Payload) {
  return BuildHttpResponse(Status, "application/json; charset=utf-8", Payload);
}

std::string Base64Encode(const unsigned char *Data, size_t Size) {
  static constexpr char Alphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  std::string Encoded;
  Encoded.reserve(((Size + 2u) / 3u) * 4u);
  for (size_t Index = 0; Index < Size; Index += 3u) {
    const uint32_t Byte0 = Data[Index];
    const uint32_t Byte1 = Index + 1u < Size ? Data[Index + 1u] : 0u;
    const uint32_t Byte2 = Index + 2u < Size ? Data[Index + 2u] : 0u;
    const uint32_t Triple = (Byte0 << 16u) | (Byte1 << 8u) | Byte2;

    Encoded.push_back(Alphabet[(Triple >> 18u) & 0x3Fu]);
    Encoded.push_back(Alphabet[(Triple >> 12u) & 0x3Fu]);
    Encoded.push_back(Index + 1u < Size ? Alphabet[(Triple >> 6u) & 0x3Fu]
                                        : '=');
    Encoded.push_back(Index + 2u < Size ? Alphabet[Triple & 0x3Fu] : '=');
  }
  return Encoded;
}

std::optional<std::array<unsigned char, 20>>
ComputeSha1(std::string_view Input) {
  std::array<unsigned char, 20> Digest{};
  uint64_t BitLength = static_cast<uint64_t>(Input.size()) * 8u;
  std::vector<unsigned char> Buffer(Input.begin(), Input.end());
  Buffer.push_back(0x80u);
  while ((Buffer.size() % 64u) != 56u) {
    Buffer.push_back(0u);
  }
  for (int Shift = 56; Shift >= 0; Shift -= 8) {
    Buffer.push_back(
        static_cast<unsigned char>((BitLength >> Shift) & 0xFFu));
  }

  uint32_t H0 = 0x67452301u;
  uint32_t H1 = 0xEFCDAB89u;
  uint32_t H2 = 0x98BADCFEu;
  uint32_t H3 = 0x10325476u;
  uint32_t H4 = 0xC3D2E1F0u;

  auto LeftRotate = [](uint32_t Value, int Shift) {
    return static_cast<uint32_t>((Value << Shift) | (Value >> (32 - Shift)));
  };

  for (size_t ChunkOffset = 0; ChunkOffset < Buffer.size();
       ChunkOffset += 64u) {
    std::array<uint32_t, 80> Words{};
    for (size_t Index = 0; Index < 16u; ++Index) {
      const size_t Base = ChunkOffset + (Index * 4u);
      Words[Index] = (static_cast<uint32_t>(Buffer[Base]) << 24u) |
                     (static_cast<uint32_t>(Buffer[Base + 1u]) << 16u) |
                     (static_cast<uint32_t>(Buffer[Base + 2u]) << 8u) |
                     static_cast<uint32_t>(Buffer[Base + 3u]);
    }
    for (size_t Index = 16u; Index < Words.size(); ++Index) {
      Words[Index] = LeftRotate(
          Words[Index - 3u] ^ Words[Index - 8u] ^ Words[Index - 14u] ^
              Words[Index - 16u],
          1);
    }

    uint32_t A = H0;
    uint32_t B = H1;
    uint32_t C = H2;
    uint32_t D = H3;
    uint32_t E = H4;

    for (size_t Index = 0; Index < Words.size(); ++Index) {
      uint32_t F = 0;
      uint32_t K = 0;
      if (Index < 20u) {
        F = (B & C) | ((~B) & D);
        K = 0x5A827999u;
      } else if (Index < 40u) {
        F = B ^ C ^ D;
        K = 0x6ED9EBA1u;
      } else if (Index < 60u) {
        F = (B & C) | (B & D) | (C & D);
        K = 0x8F1BBCDCu;
      } else {
        F = B ^ C ^ D;
        K = 0xCA62C1D6u;
      }

      const uint32_t Temp =
          LeftRotate(A, 5) + F + E + K + Words[Index];
      E = D;
      D = C;
      C = LeftRotate(B, 30);
      B = A;
      A = Temp;
    }

    H0 += A;
    H1 += B;
    H2 += C;
    H3 += D;
    H4 += E;
  }

  const std::array<uint32_t, 5> State{H0, H1, H2, H3, H4};
  for (size_t Index = 0; Index < State.size(); ++Index) {
    const uint32_t Word = State[Index];
    Digest[Index * 4u] = static_cast<unsigned char>((Word >> 24u) & 0xFFu);
    Digest[Index * 4u + 1u] =
        static_cast<unsigned char>((Word >> 16u) & 0xFFu);
    Digest[Index * 4u + 2u] =
        static_cast<unsigned char>((Word >> 8u) & 0xFFu);
    Digest[Index * 4u + 3u] = static_cast<unsigned char>(Word & 0xFFu);
  }

  return Digest;
}

bool IsWebSocketUpgradeRequest(std::string_view HeaderBlock) {
  const auto Upgrade = FindHeaderValue(HeaderBlock, "Upgrade");
  const auto Connection = FindHeaderValue(HeaderBlock, "Connection");
  std::string LowerConnection =
      Connection.has_value() ? *Connection : std::string();
  std::transform(LowerConnection.begin(), LowerConnection.end(),
                 LowerConnection.begin(),
                 [](unsigned char Character) {
                   return static_cast<char>(std::tolower(Character));
                 });
  return Upgrade.has_value() && Connection.has_value() &&
         EqualsCaseInsensitive(*Upgrade, "websocket") &&
         LowerConnection.find("upgrade") != std::string::npos;
}

std::vector<unsigned char> BuildWebSocketFrame(uint8_t Opcode, const void *Data,
                                               size_t Size) {
  std::vector<unsigned char> Frame;
  Frame.reserve(Size + 10u);
  Frame.push_back(static_cast<unsigned char>(0x80u | (Opcode & 0x0Fu)));
  if (Size <= 125u) {
    Frame.push_back(static_cast<unsigned char>(Size));
  } else if (Size <= 0xFFFFu) {
    Frame.push_back(126u);
    Frame.push_back(static_cast<unsigned char>((Size >> 8u) & 0xFFu));
    Frame.push_back(static_cast<unsigned char>(Size & 0xFFu));
  } else {
    Frame.push_back(127u);
    for (int Shift = 56; Shift >= 0; Shift -= 8) {
      Frame.push_back(
          static_cast<unsigned char>((static_cast<uint64_t>(Size) >> Shift) &
                                     0xFFu));
    }
  }

  const auto *Bytes = static_cast<const unsigned char *>(Data);
  Frame.insert(Frame.end(), Bytes, Bytes + Size);
  return Frame;
}

std::string GenerateClientId() {
  static std::atomic<uint64_t> Counter{1};
  const uint64_t Value = Counter.fetch_add(1);
  std::ostringstream Stream;
  Stream << "client-" << Value;
  return Stream.str();
}
} // namespace

struct RemoteViewportServer::RemoteClientSession::PacketOutput final
    : IEncodedVideoPacketOutput {
  PacketOutput(RemoteViewportServer &ServerIn, std::string ClientIdIn)
      : Server(ServerIn), ClientId(std::move(ClientIdIn)) {}

  RemoteViewportServer &Server;
  std::string ClientId;

  void OnEncodedVideoPacket(const EncodedVideoPacket &Packet) override {
    Server.HandleClientEncodedVideoPacket(ClientId, Packet);
  }
};

RemoteViewportServer::RemoteViewportServer(
    HeadlessSessionHost &Host, const RemoteViewportServerOptions &Options)
    : m_Host(Host), m_Options(Options) {
  m_Host.GetHeadlessLayer().SetRenderFrameObserver(
      [this](uint64_t FrameIndex, SessionUserId User) {
        RecordRenderFrameTarget(FrameIndex, User);
      });
}

RemoteViewportServer::~RemoteViewportServer() {
  m_Host.GetHeadlessLayer().SetRenderFrameObserver({});
  Stop();
}

bool RemoteViewportServer::Start(std::string &Error) {
#if AXIOM_PLATFORM_WINDOWS
  WinsockRuntime Winsock;
  (void)Winsock;
#endif

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

  SocketHandle ListenSocket = InvalidSocket;
  for (addrinfo *Current = AddressInfo; Current != nullptr;
       Current = Current->ai_next) {
    ListenSocket =
        socket(Current->ai_family, Current->ai_socktype, Current->ai_protocol);
    if (ListenSocket == InvalidSocket) {
      continue;
    }

    SetReuseAddress(ListenSocket);

    if (bind(ListenSocket, Current->ai_addr, Current->ai_addrlen) == 0 &&
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
}

void RemoteViewportServer::Stop() {
  const bool WasStopping = m_StopRequested.exchange(true);
  if (WasStopping) {
    return;
  }

  const SocketHandle ListenSocket = ToSocket(m_ListenSocket);
  m_ListenSocket = ToValue(InvalidSocket);
  CloseSocket(ListenSocket);
  CloseAllClients();
  if (m_AcceptThread.joinable()) {
    m_AcceptThread.join();
  }

  m_Host.GetTransport().Disconnect(this);
  for (IWebRtcSession *Session : CollectClientWebRtcSessions()) {
    Session->ResetPeer("server_stopped");
  }
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
  BroadcastTextMessage(Message);
  for (IWebRtcSession *Session : CollectClientWebRtcSessions()) {
    Session->SendReliableMessage(Message);
  }
}

void RemoteViewportServer::OnSessionTransportViewportFrame(
    const ViewportFrame &Frame) {
  if (const auto ClientId = TakeNextRenderTargetClientId();
      ClientId.has_value()) {
    if (RemoteClientSession *Client = FindClientSession(*ClientId);
        Client != nullptr) {
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

  AdvanceRenderTargetForNextFrame();

  if (!ShouldPublishJpegFrames()) {
    return;
  }

  const auto Captured = ConvertFrame(Frame);
  if (!Captured.has_value()) {
    BroadcastTextMessage(SerializeError("Unsupported viewport frame format."));
    return;
  }

  SetLatestFrame(*Captured);
}

void RemoteViewportServer::OnSessionTransportEncodedVideoPacket(
    const EncodedVideoPacket &Packet) {
  SetLatestEncodedPacket(Packet);
}

void RemoteViewportServer::AcceptLoop() {
  const SocketHandle ListenSocket = ToSocket(m_ListenSocket);
  while (!m_StopRequested.load()) {
    sockaddr_in ClientAddress{};
    socklen_t ClientAddressLength = sizeof(ClientAddress);
    const SocketHandle ClientSocket =
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
}

void RemoteViewportServer::HandleClient(uintptr_t ClientSocketValue) {
  const SocketHandle ClientSocket = ToSocket(ClientSocketValue);
  if (HandleHttpRequest(ClientSocketValue)) {
    return;
  }
  CloseSocket(ClientSocket);
}

void RemoteViewportServer::BroadcastTextMessage(std::string Message) {
  std::vector<uintptr_t> Clients;
  {
    std::scoped_lock Lock(m_ClientMutex);
    for (const auto &Client : m_WebSocketClients) {
      if (Client.IsOpen) {
        Clients.push_back(Client.SocketValue);
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
    RemoveWebSocketClient(FailedClient);
  }
}

void RemoteViewportServer::BroadcastBinaryFrame(const LatestFrame &Frame) {
  std::vector<uintptr_t> Clients;
  {
    std::scoped_lock Lock(m_ClientMutex);
    for (const auto &Client : m_WebSocketClients) {
      if (Client.IsOpen) {
        Clients.push_back(Client.SocketValue);
      }
    }
  }

  std::vector<uintptr_t> FailedClients;
  for (const uintptr_t ClientSocketValue : Clients) {
    if (!SendBinaryMessage(ClientSocketValue, Frame.JpegBytes.data(),
                           Frame.JpegBytes.size())) {
      FailedClients.push_back(ClientSocketValue);
    }
  }

  for (const uintptr_t FailedClient : FailedClients) {
    RemoveWebSocketClient(FailedClient);
  }
}

void RemoteViewportServer::CloseAllClients() {
  std::vector<uintptr_t> ClientSockets;
  {
    std::scoped_lock Lock(m_ClientMutex);
    for (auto &Client : m_WebSocketClients) {
      Client.IsOpen = false;
      ClientSockets.push_back(Client.SocketValue);
    }
    m_WebSocketClients.clear();
  }

  for (const uintptr_t ClientSocketValue : ClientSockets) {
    CloseSocket(ToSocket(ClientSocketValue));
  }
}

void RemoteViewportServer::RemoveWebSocketClient(uintptr_t ClientSocketValue) {
  bool Removed = false;
  {
    std::scoped_lock Lock(m_ClientMutex);
    auto It = std::find_if(m_WebSocketClients.begin(), m_WebSocketClients.end(),
                           [ClientSocketValue](const WebSocketClient &Client) {
                             return Client.SocketValue == ClientSocketValue;
                           });
    if (It != m_WebSocketClients.end()) {
      It->IsOpen = false;
      m_WebSocketClients.erase(It);
      Removed = true;
    }
  }

  if (Removed) {
    CloseSocket(ToSocket(ClientSocketValue));
    std::cout << SerializeDisconnected() << std::endl;
  }
}

bool RemoteViewportServer::SendTextMessage(uintptr_t ClientSocketValue,
                                           std::string_view Message) {
  const auto Frame = BuildWebSocketFrame(0x1u, Message.data(), Message.size());
  std::scoped_lock Lock(m_SendMutex);
  return SendAll(ToSocket(ClientSocketValue), Frame.data(), Frame.size());
}

bool RemoteViewportServer::SendBinaryMessage(uintptr_t ClientSocketValue,
                                             const void *Data, size_t Size) {
  const auto Frame = BuildWebSocketFrame(0x2u, Data, Size);
  std::scoped_lock Lock(m_SendMutex);
  return SendAll(ToSocket(ClientSocketValue), Frame.data(), Frame.size());
}

bool RemoteViewportServer::HandleHttpRequest(uintptr_t ClientSocketValue) {
  const SocketHandle ClientSocket = ToSocket(ClientSocketValue);
  HttpRequest Request{};
  std::string Error;
  if (!ReadHttpRequest(ClientSocket, Request, Error)) {
    const std::string Response =
        JsonResponse("400 Bad Request", SerializeError(Error));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  if (Request.Method == "GET" &&
      HandleWebSocketUpgrade(ClientSocketValue, Request.HeaderBlock,
                             Request.Path)) {
    return true;
  }
  if (Request.Method == "GET") {
    return HandleGetRequest(ClientSocketValue, Request.Path, Request.HeaderBlock);
  }
  if (Request.Method == "POST") {
    return HandlePostRequest(ClientSocketValue, Request.Path, Request.HeaderBlock,
                             Request.Body);
  }
  if (Request.Method == "OPTIONS") {
    const std::string Response = BuildHttpResponse(
        "204 No Content", "text/plain; charset=utf-8", "",
        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type, X-Axiom-Client-Id\r\n");
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  const std::string Response = JsonResponse(
      "405 Method Not Allowed",
      SerializeError("Only GET, POST, and OPTIONS are supported."));
  SendAll(ClientSocket, Response.data(), Response.size());
  return false;
}

bool RemoteViewportServer::HandlePostRequest(uintptr_t ClientSocketValue,
                                             std::string_view Path,
                                             std::string_view HeaderBlock,
                                             std::string_view Body) {
  const SocketHandle ClientSocket = ToSocket(ClientSocketValue);
  const std::string_view Route = StripQuery(Path);
  if (Route == "/session/connect") {
    return HandleSessionConnectRequest(ClientSocketValue, HeaderBlock, Body);
  }
  if (Route == "/webrtc/offer") {
    return HandleWebRtcOfferRequest(ClientSocketValue, HeaderBlock, Body);
  }
  if (Route == "/webrtc/ice-candidate") {
    return HandleWebRtcIceCandidateRequest(ClientSocketValue, HeaderBlock, Body);
  }
  if (Route == "/webrtc/close") {
    return HandleWebRtcCloseRequest(ClientSocketValue, HeaderBlock, Body);
  }
  if (Route != "/command") {
    const std::string Response =
        JsonResponse("404 Not Found", SerializeError("Unknown POST endpoint."));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  const auto User = ResolveClientUser(HeaderBlock);
  if (!User.has_value()) {
    const std::string Response =
        JsonResponse("400 Bad Request",
                     SerializeError("Missing or unknown X-Axiom-Client-Id."));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  if (const auto ClientId = FindHeaderValue(HeaderBlock, ClientIdHeaderName);
      ClientId.has_value()) {
    TouchClientSession(*ClientId);
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
  case HeadlessCommandType::SetViewportCameraPose:
  case HeadlessCommandType::UpdateViewportCamera:
  case HeadlessCommandType::SelectObject:
  case HeadlessCommandType::SetTransform:
    m_Host.SubmitRemoteCommand(*User, Command->EditorPayload);
    break;
  case HeadlessCommandType::Quit:
    m_StopRequested.store(true);
    m_Host.RequestClose();
    BroadcastTextMessage(SerializeShutdown());
    break;
  case HeadlessCommandType::LoadStartupScene:
  case HeadlessCommandType::RenderFrame:
    break;
  }

  const std::string Response =
      JsonResponse("202 Accepted", "{\"type\":\"accepted\"}");
  SendAll(ClientSocket, Response.data(), Response.size());
  return false;
}

bool RemoteViewportServer::HandleSessionConnectRequest(
    uintptr_t ClientSocketValue, std::string_view HeaderBlock,
    std::string_view Body) {
  (void)Body;
  const SocketHandle ClientSocket = ToSocket(ClientSocketValue);
  const auto ClientIdHint = FindHeaderValue(HeaderBlock, ClientIdHeaderName);
  const ClientSessionResolution Resolution =
      CreateOrResumeClientSession(ClientIdHint);
  RemoteClientSession &Client = *Resolution.Session;
  TouchClientSession(Client.ClientId);
  AdvanceRenderTargetForNextFrame();

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
  const std::string Payload = SerializeSessionConnectResponse(
      Client.ClientId, m_Host.GetHeadlessLayer().GetSession().GetState(),
      Client.User, m_TransportConnected.load(),
      m_TransportConnected.load() ? "connected" : "disconnected",
      Status.ConnectionState);
  const std::string Response = JsonResponse("200 OK", Payload);
  SendAll(ClientSocket, Response.data(), Response.size());
  return false;
}

bool RemoteViewportServer::HandleGetRequest(uintptr_t ClientSocketValue,
                                            std::string_view Path,
                                            std::string_view HeaderBlock) {
  const SocketHandle ClientSocket = ToSocket(ClientSocketValue);
  const std::string_view Route = StripQuery(Path);
  if (Route == "/health") {
    const std::string Body = SerializeReady(m_Options.Width, m_Options.Height);
    const std::string Response = JsonResponse("200 OK", Body);
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }
  if (Route == "/webrtc") {
    const auto ClientId = FindHeaderValue(HeaderBlock, ClientIdHeaderName);
    const WebRtcSessionStatus Status =
        ClientId.has_value() ? GetClientWebRtcStatus(*ClientId)
                             : WebRtcSessionStatus{};
    const std::string Body =
        SerializeWebRtcStatus(Status.Enabled, Status.Available,
                              Status.SignalingState, Status.ConnectionState,
                              Status.Detail, Status.SessionId,
                              Status.PendingLocalIceCandidateCount,
                              Status.Video);
    const std::string Response = JsonResponse("200 OK", Body);
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }
  if (Route == "/webrtc/ice-candidates") {
    const auto ClientId = FindHeaderValue(HeaderBlock, ClientIdHeaderName);
    std::vector<WebRtcIceCandidate> Candidates;
    if (ClientId.has_value()) {
      if (RemoteClientSession *Client = FindClientSession(*ClientId);
          Client != nullptr && Client->WebRtcSession != nullptr) {
        Candidates = Client->WebRtcSession->TakePendingLocalIceCandidates();
      }
    }
    const std::string Body = SerializeWebRtcIceCandidateList(Candidates);
    const std::string Response = JsonResponse("200 OK", Body);
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }
  if (Route == "/session") {
    const auto User = ResolveClientUser(HeaderBlock);
    const auto ClientId = FindHeaderValue(HeaderBlock, ClientIdHeaderName);
    if (!User.has_value()) {
      const std::string Response = JsonResponse(
          "400 Bad Request",
          SerializeError("Missing or unknown X-Axiom-Client-Id."));
      SendAll(ClientSocket, Response.data(), Response.size());
      return false;
    }
    if (ClientId.has_value()) {
      TouchClientSession(*ClientId);
    }
    const WebRtcSessionStatus Status =
        ClientId.has_value() ? GetClientWebRtcStatus(*ClientId)
                             : WebRtcSessionStatus{};
    const std::string Body = SerializeSessionSnapshot(
        m_Host.GetHeadlessLayer().GetSession().GetState(), *User,
        m_TransportConnected.load(),
        m_TransportConnected.load() ? "connected" : "disconnected",
        Status.ConnectionState);
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
        BuildBinaryResponse("200 OK", "image/jpeg", Frame.JpegBytes.size());
    if (!SendAll(ClientSocket, Headers.data(), Headers.size())) {
      return false;
    }
    SendAll(ClientSocket, Frame.JpegBytes.data(), Frame.JpegBytes.size());
    return false;
  }
  if (Route == "/h264/metadata") {
    LatestEncodedPacket Packet{};
    if (!TryGetLatestEncodedPacket(Packet)) {
      const std::string Response = JsonResponse(
          "404 Not Found",
          SerializeError("No encoded H.264 packet is available yet."));
      SendAll(ClientSocket, Response.data(), Response.size());
      return false;
    }

    const std::string Body =
        SerializeEncodedVideoPacketMetadata(Packet.Packet, "/h264");
    const std::string Response = JsonResponse("200 OK", Body);
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }
  if (Route == "/h264") {
    LatestEncodedPacket Packet{};
    if (!TryGetLatestEncodedPacket(Packet)) {
      const std::string Response = JsonResponse(
          "404 Not Found",
          SerializeError("No encoded H.264 packet is available yet."));
      SendAll(ClientSocket, Response.data(), Response.size());
      return false;
    }

    const std::string Headers =
        BuildBinaryResponse("200 OK", "video/h264", Packet.Packet.Bytes.size());
    if (!SendAll(ClientSocket, Headers.data(), Headers.size())) {
      return false;
    }
    SendAll(ClientSocket, Packet.Packet.Bytes.data(), Packet.Packet.Bytes.size());
    return false;
  }

  const std::string Response =
      JsonResponse("404 Not Found", SerializeError("Unknown GET endpoint."));
  SendAll(ClientSocket, Response.data(), Response.size());
  return false;
}

bool RemoteViewportServer::HandleWebRtcOfferRequest(uintptr_t ClientSocketValue,
                                                    std::string_view HeaderBlock,
                                                    std::string_view Body) {
  const SocketHandle ClientSocket = ToSocket(ClientSocketValue);
  const auto User = ResolveClientUser(HeaderBlock);
  const auto ClientId = FindHeaderValue(HeaderBlock, ClientIdHeaderName);
  if (!User.has_value()) {
    const std::string Response =
        JsonResponse("400 Bad Request",
                     SerializeError("Missing or unknown X-Axiom-Client-Id."));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }
  if (ClientId.has_value()) {
    TouchClientSession(*ClientId);
  }

  std::string Error;
  const auto Offer = ParseWebRtcSessionDescription(Body, Error);
  if (!Offer.has_value()) {
    const std::string Response =
        JsonResponse("400 Bad Request", SerializeError(Error));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  if (Offer->Type != "offer") {
    const std::string Response = JsonResponse(
        "400 Bad Request",
        SerializeError("WebRTC offer endpoint requires `type` to be `offer`."));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  if (!ClientId.has_value()) {
    const std::string Response = JsonResponse(
        "503 Service Unavailable",
        SerializeError("Missing X-Axiom-Client-Id for WebRTC session."));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }
  RemoteClientSession *Client = FindClientSession(*ClientId);
  if (Client == nullptr || Client->WebRtcSession == nullptr) {
    const std::string Response = JsonResponse(
        "503 Service Unavailable",
        SerializeError("WebRTC session support is unavailable."));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  WebRtcSessionDescription Answer{};
  if (!Client->WebRtcSession->HandleOffer(*Offer, Answer, Error)) {
    const WebRtcSessionStatus Status = Client->WebRtcSession->GetStatus();
    const std::string Payload = SerializeWebRtcStatus(
        Status.Enabled, Status.Available, Status.SignalingState,
        Status.ConnectionState, Error.empty() ? Status.Detail : Error,
        Status.SessionId, Status.PendingLocalIceCandidateCount,
        Status.Video);
    const std::string Response =
        JsonResponse("503 Service Unavailable", Payload);
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  AdvanceRenderTargetForNextFrame();

  const std::string Response =
      JsonResponse("200 OK", SerializeWebRtcSessionDescription(
                                 Answer, Client->WebRtcSession->GetStatus().SessionId));
  SendAll(ClientSocket, Response.data(), Response.size());
  return false;
}

bool RemoteViewportServer::HandleWebRtcIceCandidateRequest(
    uintptr_t ClientSocketValue, std::string_view HeaderBlock,
    std::string_view Body) {
  const SocketHandle ClientSocket = ToSocket(ClientSocketValue);
  const auto User = ResolveClientUser(HeaderBlock);
  if (!User.has_value()) {
    const std::string Response =
        JsonResponse("400 Bad Request",
                     SerializeError("Missing or unknown X-Axiom-Client-Id."));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }
  if (const auto ClientId = FindHeaderValue(HeaderBlock, ClientIdHeaderName);
      ClientId.has_value()) {
    TouchClientSession(*ClientId);
  }

  std::string Error;
  const auto Candidate = ParseWebRtcIceCandidate(Body, Error);
  if (!Candidate.has_value()) {
    const std::string Response =
        JsonResponse("400 Bad Request", SerializeError(Error));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  const auto ClientId = FindHeaderValue(HeaderBlock, ClientIdHeaderName);
  if (!ClientId.has_value()) {
    const std::string Response = JsonResponse(
        "503 Service Unavailable",
        SerializeError("Missing X-Axiom-Client-Id for WebRTC session."));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }
  RemoteClientSession *Client = FindClientSession(*ClientId);
  if (Client == nullptr || Client->WebRtcSession == nullptr) {
    const std::string Response = JsonResponse(
        "503 Service Unavailable",
        SerializeError("WebRTC session support is unavailable."));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  if (!Client->WebRtcSession->AddRemoteIceCandidate(*Candidate, Error)) {
    const WebRtcSessionStatus Status = Client->WebRtcSession->GetStatus();
    const std::string Payload = SerializeWebRtcStatus(
        Status.Enabled, Status.Available, Status.SignalingState,
        Status.ConnectionState, Error.empty() ? Status.Detail : Error,
        Status.SessionId, Status.PendingLocalIceCandidateCount,
        Status.Video);
    const std::string Response =
        JsonResponse("503 Service Unavailable", Payload);
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  const std::string Response =
      JsonResponse("202 Accepted", "{\"type\":\"accepted\"}");
  SendAll(ClientSocket, Response.data(), Response.size());
  return false;
}

bool RemoteViewportServer::HandleWebRtcCloseRequest(
    uintptr_t ClientSocketValue, std::string_view HeaderBlock,
    std::string_view Body) {
  const SocketHandle ClientSocket = ToSocket(ClientSocketValue);
  const auto User = ResolveClientUser(HeaderBlock);
  const auto ClientId = FindHeaderValue(HeaderBlock, ClientIdHeaderName);
  if (!User.has_value()) {
    const std::string Response =
        JsonResponse("400 Bad Request",
                     SerializeError("Missing or unknown X-Axiom-Client-Id."));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }
  if (ClientId.has_value()) {
    TouchClientSession(*ClientId);
  }

  std::string Reason = "browser_requested_close";
  if (!Body.empty()) {
    Reason = std::string(Body);
  }

  if (!ClientId.has_value()) {
    const std::string Response = JsonResponse(
        "503 Service Unavailable",
        SerializeError("Missing X-Axiom-Client-Id for WebRTC session."));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }
  RemoteClientSession *Client = FindClientSession(*ClientId);
  if (Client == nullptr || Client->WebRtcSession == nullptr) {
    const std::string Response = JsonResponse(
        "503 Service Unavailable",
        SerializeError("WebRTC session support is unavailable."));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  std::string Error;
  if (!Client->WebRtcSession->CloseSession(Reason, Error)) {
    const std::string Response =
        JsonResponse("500 Internal Server Error",
                     SerializeError(Error.empty()
                                        ? "Failed to close WebRTC session."
                                        : Error));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  if (ClientId.has_value()) {
    std::optional<SessionUserId> DisconnectedUser;
    {
      std::scoped_lock Lock(m_ClientMutex);
      const auto It = m_RemoteClientsById.find(*ClientId);
      if (It != m_RemoteClientsById.end()) {
        DisconnectedUser = It->second.User;
      }
    }
    if (DisconnectedUser.has_value()) {
      m_Host.GetHeadlessLayer().GetSession().SetPresenceState(
          *DisconnectedUser, EditorUserPresenceState::Disconnected);
    }
  }
  m_Host.SetActiveRenderUser(
      m_Host.GetHeadlessLayer().GetLocalUserId());
  AdvanceRenderTargetForNextFrame();
  const WebRtcSessionStatus Status = Client->WebRtcSession->GetStatus();
  const std::string Payload = SerializeWebRtcStatus(
      Status.Enabled, Status.Available, Status.SignalingState,
      Status.ConnectionState, Status.Detail, Status.SessionId,
      Status.PendingLocalIceCandidateCount, Status.Video);
  const std::string Response = JsonResponse("200 OK", Payload);
  SendAll(ClientSocket, Response.data(), Response.size());
  return false;
}

std::optional<SessionUserId> RemoteViewportServer::ResolveClientUser(
    std::string_view HeaderBlock) const {
  const auto ClientId = FindHeaderValue(HeaderBlock, ClientIdHeaderName);
  if (!ClientId.has_value()) {
    return std::nullopt;
  }

  std::scoped_lock Lock(m_ClientMutex);
  const auto It = m_RemoteClientsById.find(*ClientId);
  if (It == m_RemoteClientsById.end()) {
    return std::nullopt;
  }
  return It->second.User;
}

RemoteViewportServer::RemoteClientSession *
RemoteViewportServer::FindClientSession(std::string_view ClientId) {
  std::scoped_lock Lock(m_ClientMutex);
  const auto It = m_RemoteClientsById.find(std::string(ClientId));
  return It != m_RemoteClientsById.end() ? &It->second : nullptr;
}

const RemoteViewportServer::RemoteClientSession *
RemoteViewportServer::FindClientSession(std::string_view ClientId) const {
  std::scoped_lock Lock(m_ClientMutex);
  const auto It = m_RemoteClientsById.find(std::string(ClientId));
  return It != m_RemoteClientsById.end() ? &It->second : nullptr;
}

WebRtcSessionStatus
RemoteViewportServer::GetClientWebRtcStatus(std::string_view ClientId) const {
  std::scoped_lock Lock(m_ClientMutex);
  const auto It = m_RemoteClientsById.find(std::string(ClientId));
  if (It == m_RemoteClientsById.end() || It->second.WebRtcSession == nullptr) {
    return {};
  }
  return It->second.WebRtcSession->GetStatus();
}

std::vector<IWebRtcSession *> RemoteViewportServer::CollectClientWebRtcSessions() const {
  std::vector<IWebRtcSession *> Sessions;
  std::scoped_lock Lock(m_ClientMutex);
  Sessions.reserve(m_RemoteClientsById.size());
  for (const auto &[ClientId, Session] : m_RemoteClientsById) {
    (void)ClientId;
    if (Session.WebRtcSession != nullptr) {
      Sessions.push_back(Session.WebRtcSession.get());
    }
  }
  return Sessions;
}

RemoteViewportServer::ClientSessionResolution
RemoteViewportServer::CreateOrResumeClientSession(
    const std::optional<std::string> &ClientIdHint) {
  RemoteClientSession *ResolvedSession = nullptr;
  bool ResumedExisting = false;
  {
    std::scoped_lock Lock(m_ClientMutex);
    if (ClientIdHint.has_value()) {
      const auto Existing = m_RemoteClientsById.find(*ClientIdHint);
      if (Existing != m_RemoteClientsById.end()) {
        Existing->second.LastActivity = std::chrono::steady_clock::now();
        ResolvedSession = &Existing->second;
        ResumedExisting = true;
      }
    }
    if (ResolvedSession == nullptr) {
      RemoteClientSession Session{};
      Session.ClientId = GenerateClientId();
      Session.User = SessionUserId{m_NextRemoteUserId++};
      Session.LastActivity = std::chrono::steady_clock::now();
      Session.WebRtcSession = CreateWebRtcSession();
      Session.VideoEncoder = CreateDefaultVideoEncoder();
      Session.VideoPacketOutput = std::make_unique<RemoteClientSession::PacketOutput>(
          *this, Session.ClientId);
      if (Session.VideoEncoder != nullptr && Session.VideoPacketOutput != nullptr) {
        Session.VideoEncoder->SetOutput(Session.VideoPacketOutput.get());
      }
      if (Session.WebRtcSession != nullptr) {
        const std::string ClientId = Session.ClientId;
        Session.WebRtcSession->SetCommandMessageHandler(
            [this, ClientId](std::string_view Payload) {
              HandleClientWebRtcMessage(ClientId, Payload);
            });
      }
      auto [It, Inserted] =
          m_RemoteClientsById.emplace(Session.ClientId, std::move(Session));
      (void)Inserted;
      ResolvedSession = &It->second;
    }
  }

  m_Host.GetHeadlessLayer().GetSession().EnsureViewportState(ResolvedSession->User);
  m_Host.GetHeadlessLayer().GetSession().SetPresenceState(
      ResolvedSession->User, EditorUserPresenceState::Connected);
  return {.Session = ResolvedSession, .ResumedExisting = ResumedExisting};
}

void RemoteViewportServer::TouchClientSession(const std::string &ClientId) {
  std::scoped_lock Lock(m_ClientMutex);
  const auto It = m_RemoteClientsById.find(ClientId);
  if (It != m_RemoteClientsById.end()) {
    It->second.LastActivity = std::chrono::steady_clock::now();
  }
}

void RemoteViewportServer::RecordRenderFrameTarget(uint64_t FrameIndex,
                                                   SessionUserId User) {
  (void)FrameIndex;
  std::scoped_lock Lock(m_FrameMutex, m_ClientMutex);
  auto ClientIt = std::find_if(
      m_RemoteClientsById.begin(), m_RemoteClientsById.end(),
      [User](const auto &Entry) { return Entry.second.User.Value == User.Value; });
  if (ClientIt == m_RemoteClientsById.end()) {
    return;
  }

  m_PendingRenderTargetClientIds.push_back(ClientIt->second.ClientId);
  if (m_PendingRenderTargetClientIds.size() > 16u) {
    m_PendingRenderTargetClientIds.erase(m_PendingRenderTargetClientIds.begin());
  }
}

void RemoteViewportServer::AdvanceRenderTargetForNextFrame() {
  struct RenderCandidate {
    std::string ClientId;
    SessionUserId User;
  };

  std::optional<RenderCandidate> Selected;
  {
    std::scoped_lock Lock(m_ClientMutex);
    std::vector<RenderCandidate> Candidates;
    Candidates.reserve(m_RemoteClientsById.size());
    for (const auto &[ClientId, Session] : m_RemoteClientsById) {
      if (Session.WebRtcSession == nullptr) {
        continue;
      }

      const WebRtcSessionStatus Status = Session.WebRtcSession->GetStatus();
      if (!Status.Enabled || !Status.Available ||
          Status.ConnectionState != "connected") {
        continue;
      }

      Candidates.push_back({.ClientId = ClientId, .User = Session.User});
    }

    std::sort(Candidates.begin(), Candidates.end(),
              [](const RenderCandidate &Left, const RenderCandidate &Right) {
                return Left.User.Value < Right.User.Value;
              });

    if (!Candidates.empty()) {
      auto Current = std::find_if(
          Candidates.begin(), Candidates.end(),
          [this](const RenderCandidate &Candidate) {
            return Candidate.ClientId == m_NextRenderClientId;
          });
      if (Current == Candidates.end()) {
        Selected = Candidates.front();
      } else {
        ++Current;
        Selected = Current == Candidates.end() ? Candidates.front() : *Current;
      }
      m_NextRenderClientId = Selected->ClientId;
    } else {
      m_NextRenderClientId.clear();
    }
  }

  m_Host.SetActiveRenderUser(Selected.has_value()
                                 ? Selected->User
                                 : m_Host.GetHeadlessLayer().GetLocalUserId());
}

std::optional<std::string> RemoteViewportServer::TakeNextRenderTargetClientId() {
  std::scoped_lock Lock(m_FrameMutex);
  if (m_PendingRenderTargetClientIds.empty()) {
    return std::nullopt;
  }
  const std::string ClientId = m_PendingRenderTargetClientIds.front();
  m_PendingRenderTargetClientIds.erase(m_PendingRenderTargetClientIds.begin());
  return ClientId;
}

void RemoteViewportServer::HandleClientEncodedVideoPacket(
    std::string_view ClientId, const EncodedVideoPacket &Packet) {
  SetLatestEncodedPacket(Packet);
  if (RemoteClientSession *Client = FindClientSession(ClientId);
      Client != nullptr && Client->WebRtcSession != nullptr) {
    Client->WebRtcSession->OnEncodedVideoPacket(Packet);
  }
}

bool RemoteViewportServer::HandleWebSocketUpgrade(uintptr_t ClientSocketValue,
                                                  std::string_view HeaderBlock,
                                                  std::string_view Path) {
  const std::string_view Route = StripQuery(Path);
  if (Route != "/ws" || !IsWebSocketUpgradeRequest(HeaderBlock)) {
    return false;
  }

  const auto Key = FindHeaderValue(HeaderBlock, "Sec-WebSocket-Key");
  if (!Key.has_value()) {
    const std::string Response = JsonResponse(
        "400 Bad Request",
        SerializeError("Missing Sec-WebSocket-Key for WebSocket upgrade."));
    const SocketHandle ClientSocket = ToSocket(ClientSocketValue);
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  const auto Digest = ComputeSha1(*Key + std::string(WebSocketGuid));
  if (!Digest.has_value()) {
    const std::string Response = JsonResponse(
        "500 Internal Server Error",
        SerializeError("Failed to compute WebSocket handshake."));
    const SocketHandle ClientSocket = ToSocket(ClientSocketValue);
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  const std::string Accept = Base64Encode(Digest->data(), Digest->size());
  std::ostringstream Response;
  Response << "HTTP/1.1 101 Switching Protocols\r\n"
           << "Upgrade: websocket\r\n"
           << "Connection: Upgrade\r\n"
           << "Sec-WebSocket-Accept: " << Accept << "\r\n\r\n";
  const std::string ResponseText = Response.str();
  const SocketHandle ClientSocket = ToSocket(ClientSocketValue);
  if (!SendAll(ClientSocket, ResponseText.data(), ResponseText.size())) {
    return false;
  }

  {
    std::scoped_lock Lock(m_ClientMutex);
    m_WebSocketClients.push_back({.SocketValue = ClientSocketValue});
  }

  std::cout << SerializeConnected() << std::endl;
  SendTextMessage(ClientSocketValue,
                  SerializeReady(m_Options.Width, m_Options.Height));
  SendTextMessage(ClientSocketValue, SerializeConnected());
  LatestFrame Frame{};
  if (TryGetLatestFrame(Frame)) {
    SendTextMessage(ClientSocketValue,
                    SerializeFrameMetadata(Frame.FrameIndex, Frame.Width,
                                           Frame.Height, "jpeg"));
    SendBinaryMessage(ClientSocketValue, Frame.JpegBytes.data(),
                      Frame.JpegBytes.size());
  }

  RunWebSocketSession(ClientSocketValue);
  return true;
}

void RemoteViewportServer::RunWebSocketSession(uintptr_t ClientSocketValue) {
  const SocketHandle ClientSocket = ToSocket(ClientSocketValue);
  while (!m_StopRequested.load()) {
    std::array<unsigned char, 2> Header{};
    if (!RecvExact(ClientSocket, Header.data(), Header.size())) {
      break;
    }

    const bool IsFinal = (Header[0] & 0x80u) != 0u;
    const uint8_t Opcode = static_cast<uint8_t>(Header[0] & 0x0Fu);
    const bool IsMasked = (Header[1] & 0x80u) != 0u;
    uint64_t PayloadLength = static_cast<uint64_t>(Header[1] & 0x7Fu);
    if (!IsFinal || !IsMasked) {
      break;
    }

    if (PayloadLength == 126u) {
      std::array<unsigned char, 2> ExtendedLength{};
      if (!RecvExact(ClientSocket, ExtendedLength.data(),
                     ExtendedLength.size())) {
        break;
      }
      PayloadLength = (static_cast<uint64_t>(ExtendedLength[0]) << 8u) |
                      static_cast<uint64_t>(ExtendedLength[1]);
    } else if (PayloadLength == 127u) {
      std::array<unsigned char, 8> ExtendedLength{};
      if (!RecvExact(ClientSocket, ExtendedLength.data(),
                     ExtendedLength.size())) {
        break;
      }
      PayloadLength = 0;
      for (const unsigned char Byte : ExtendedLength) {
        PayloadLength = (PayloadLength << 8u) | static_cast<uint64_t>(Byte);
      }
    }

    std::array<unsigned char, 4> MaskingKey{};
    if (!RecvExact(ClientSocket, MaskingKey.data(), MaskingKey.size())) {
      break;
    }

    std::vector<unsigned char> Payload(PayloadLength);
    if (PayloadLength > 0u &&
        !RecvExact(ClientSocket, Payload.data(), Payload.size())) {
      break;
    }
    for (size_t Index = 0; Index < Payload.size(); ++Index) {
      Payload[Index] ^= MaskingKey[Index % MaskingKey.size()];
    }

    if (Opcode == 0x8u) {
      break;
    }
    if (Opcode == 0x9u) {
      const auto Pong = BuildWebSocketFrame(0xAu, Payload.data(), Payload.size());
      std::scoped_lock Lock(m_SendMutex);
      if (!SendAll(ClientSocket, Pong.data(), Pong.size())) {
        break;
      }
      continue;
    }
    if (Opcode != 0x1u) {
      continue;
    }

    const std::string TextPayload(Payload.begin(), Payload.end());
    if (!HandleWebSocketMessage(TextPayload)) {
      SendTextMessage(ClientSocketValue,
                      SerializeError("Invalid WebSocket command payload."));
    }
  }

  RemoveWebSocketClient(ClientSocketValue);
}

bool RemoteViewportServer::HandleWebSocketMessage(std::string_view Payload) {
  std::string Error;
  const auto Command = ParseRemoteViewportCommand(Payload, Error);
  if (!Command.has_value()) {
    return false;
  }

  switch (Command->Type) {
  case HeadlessCommandType::SetViewMode:
    m_Host.SetRemoteViewMode(Command->ViewMode);
    return true;
  case HeadlessCommandType::SetLookActive:
  case HeadlessCommandType::SetViewportCameraPose:
  case HeadlessCommandType::SelectObject:
  case HeadlessCommandType::SetTransform:
  case HeadlessCommandType::UpdateViewportCamera:
    return false;
  case HeadlessCommandType::Quit:
    m_StopRequested.store(true);
    m_Host.RequestClose();
    BroadcastTextMessage(SerializeShutdown());
    return true;
  case HeadlessCommandType::LoadStartupScene:
  case HeadlessCommandType::RenderFrame:
    return false;
  }

  return false;
}

bool RemoteViewportServer::HandleClientWebRtcMessage(std::string_view ClientId,
                                                     std::string_view Payload) {
  std::string Error;
  const auto Command = ParseRemoteViewportCommand(Payload, Error);
  if (!Command.has_value()) {
    return false;
  }

  RemoteClientSession *Client = FindClientSession(ClientId);
  if (Client == nullptr) {
    return false;
  }
  TouchClientSession(Client->ClientId);

  switch (Command->Type) {
  case HeadlessCommandType::SetViewMode:
    m_Host.SetRemoteViewMode(Command->ViewMode);
    return true;
  case HeadlessCommandType::SetLookActive:
  case HeadlessCommandType::SetViewportCameraPose:
  case HeadlessCommandType::UpdateViewportCamera:
  case HeadlessCommandType::SelectObject:
  case HeadlessCommandType::SetTransform:
    m_Host.SubmitRemoteCommand(Client->User, Command->EditorPayload);
    return true;
  case HeadlessCommandType::Quit:
    m_StopRequested.store(true);
    m_Host.RequestClose();
    BroadcastTextMessage(SerializeShutdown());
    return true;
  case HeadlessCommandType::LoadStartupScene:
  case HeadlessCommandType::RenderFrame:
    return false;
  }

  return false;
}

bool RemoteViewportServer::ShouldPublishJpegFrames() const {
  for (IWebRtcSession *Session : CollectClientWebRtcSessions()) {
    const WebRtcSessionStatus Status = Session->GetStatus();
    if (Status.Enabled && Status.Available &&
        Status.ConnectionState == "connected") {
      return false;
    }
  }
  return true;
}

void RemoteViewportServer::SetLatestFrame(const CapturedFrame &Frame) {
  EncodedImageBuffer Buffer{};
  if (stbi_write_jpg_to_func(WriteImageBytes, &Buffer, static_cast<int>(Frame.Width),
                             static_cast<int>(Frame.Height), 4,
                             Frame.Pixels.data(), m_Options.JpegQuality) == 0) {
    BroadcastTextMessage(SerializeError("Failed to encode a JPEG frame."));
    return;
  }

  LatestFrame Latest{};
  Latest.FrameIndex = Frame.FrameIndex;
  Latest.Width = Frame.Width;
  Latest.Height = Frame.Height;
  Latest.JpegBytes = std::move(Buffer.Bytes);
  {
    std::scoped_lock Lock(m_FrameMutex);
    m_LatestFrame = Latest;
  }

  BroadcastTextMessage(SerializeFrameMetadata(Latest.FrameIndex, Latest.Width,
                                              Latest.Height, "jpeg"));
  BroadcastBinaryFrame(Latest);
}

bool RemoteViewportServer::TryGetLatestFrame(LatestFrame &Frame) const {
  std::scoped_lock Lock(m_FrameMutex);
  if (m_LatestFrame.JpegBytes.empty()) {
    return false;
  }
  Frame = m_LatestFrame;
  return true;
}

void RemoteViewportServer::SetLatestEncodedPacket(
    const EncodedVideoPacket &Packet) {
  {
    std::scoped_lock Lock(m_FrameMutex);
    m_LatestEncodedPacket.Packet = Packet;
    m_LatestEncodedPacket.HasPacket = true;
  }

  std::cout << SerializeEncodedVideoPacketMetadata(Packet, "/h264")
            << std::endl;
}

bool RemoteViewportServer::TryGetLatestEncodedPacket(
    LatestEncodedPacket &Packet) const {
  std::scoped_lock Lock(m_FrameMutex);
  if (!m_LatestEncodedPacket.HasPacket) {
    return false;
  }

  Packet = m_LatestEncodedPacket;
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
    } else if (Argument == "--jpeg-quality" && Index + 1 < argc) {
      int Quality = 0;
      const std::string_view Value(argv[++Index]);
      const auto [Ptr, Ec] =
          std::from_chars(Value.data(), Value.data() + Value.size(), Quality);
      if (Ec != std::errc{} || Ptr != Value.data() + Value.size() ||
          Quality < 1 || Quality > 100) {
        Error = "Invalid --jpeg-quality value.";
        return false;
      }
      Options.JpegQuality = Quality;
    } else {
      Error = "Unknown or incomplete argument: " + std::string(Argument);
      return false;
    }
  }

  return true;
}
} // namespace Axiom
