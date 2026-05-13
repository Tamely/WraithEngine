#include "RemoteViewportServer.h"

#include <Core/Platform.h>
#include <Project/ProjectSystem.h>

#ifndef AXIOM_CONTENT_DIR
#define AXIOM_CONTENT_DIR "Content"
#endif

#include "GizmoHitTest.h"
#include "HeadlessCommandProtocol.h"
#include <Session/MeshPicking.h>
#include <Renderer/VideoEncoderFactory.h>
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
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

std::string UrlDecode(std::string_view Input) {
  std::string Out;
  Out.reserve(Input.size());
  for (size_t I = 0; I < Input.size(); ++I) {
    if (Input[I] == '%' && I + 2 < Input.size()) {
      const char Hi = Input[I + 1];
      const char Lo = Input[I + 2];
      auto HexVal = [](char C) -> int {
        if (C >= '0' && C <= '9') return C - '0';
        if (C >= 'a' && C <= 'f') return C - 'a' + 10;
        if (C >= 'A' && C <= 'F') return C - 'A' + 10;
        return -1;
      };
      const int H = HexVal(Hi), L = HexVal(Lo);
      if (H >= 0 && L >= 0) {
        Out += static_cast<char>(H * 16 + L);
        I += 2;
        continue;
      }
    } else if (Input[I] == '+') {
      Out += ' ';
      continue;
    }
    Out += Input[I];
  }
  return Out;
}

// Returns the URL-decoded value of a query parameter, or nullopt if not present.
std::optional<std::string> GetQueryParam(std::string_view Path,
                                          std::string_view Key) {
  const size_t Q = Path.find('?');
  if (Q == std::string_view::npos) return std::nullopt;
  std::string_view Query = Path.substr(Q + 1);
  while (!Query.empty()) {
    const size_t Amp = Query.find('&');
    std::string_view Pair = Amp == std::string_view::npos ? Query : Query.substr(0, Amp);
    const size_t Eq = Pair.find('=');
    if (Eq != std::string_view::npos && Pair.substr(0, Eq) == Key) {
      return UrlDecode(Pair.substr(Eq + 1));
    }
    if (Amp == std::string_view::npos) break;
    Query.remove_prefix(Amp + 1);
  }
  return std::nullopt;
}

constexpr float kMinimumScale = 0.001f;

float SnapToStep(float value, float step) {
  if (step <= 0.0f) {
    return value;
  }
  return std::round(value / step) * step;
}

void ApplyGridSnap(bool enabled, float translationStep,
                   float rotationStepDegrees, float scaleStep, GizmoMode mode, int axis,
                   glm::vec3 &location, glm::vec3 &rotationDegrees,
                   glm::vec3 &scale) {
  if (!enabled || axis < 0 || axis > 2) {
    return;
  }

  switch (mode) {
  case GizmoMode::Translate:
    location[axis] = SnapToStep(location[axis], translationStep);
    break;
  case GizmoMode::Rotate:
    rotationDegrees[axis] = SnapToStep(rotationDegrees[axis], rotationStepDegrees);
    break;
  case GizmoMode::Scale:
    scale[axis] = std::max(kMinimumScale, SnapToStep(scale[axis], scaleStep));
    break;
  }
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

std::optional<std::string> ExtractJsonStringField(std::string_view Body,
                                                  std::string_view FieldName) {
  const std::string Needle = "\"" + std::string(FieldName) + "\"";
  const size_t KeyPos = Body.find(Needle);
  if (KeyPos == std::string_view::npos) {
    return std::nullopt;
  }

  const size_t ColonPos = Body.find(':', KeyPos + Needle.size());
  if (ColonPos == std::string_view::npos) {
    return std::nullopt;
  }

  size_t ValuePos = ColonPos + 1;
  while (ValuePos < Body.size() &&
         std::isspace(static_cast<unsigned char>(Body[ValuePos])) != 0) {
    ++ValuePos;
  }
  if (ValuePos >= Body.size() || Body[ValuePos] != '"') {
    return std::nullopt;
  }
  ++ValuePos;

  std::string Result;
  while (ValuePos < Body.size()) {
    const char Character = Body[ValuePos++];
    if (Character == '"') {
      return Result;
    }
    if (Character == '\\') {
      if (ValuePos >= Body.size()) {
        return std::nullopt;
      }
      const char Escaped = Body[ValuePos++];
      switch (Escaped) {
      case '\\':
      case '"':
      case '/':
        Result.push_back(Escaped);
        break;
      case 'n':
        Result.push_back('\n');
        break;
      case 'r':
        Result.push_back('\r');
        break;
      case 't':
        Result.push_back('\t');
        break;
      default:
        return std::nullopt;
      }
      continue;
    }
    Result.push_back(Character);
  }

  return std::nullopt;
}

std::string EscapeJsonString(std::string_view Value) {
  std::string Result;
  Result.reserve(Value.size() + 4);
  for (const char Character : Value) {
    switch (Character) {
    case '\\':
      Result += "\\\\";
      break;
    case '"':
      Result += "\\\"";
      break;
    case '\n':
      Result += "\\n";
      break;
    case '\r':
      Result += "\\r";
      break;
    case '\t':
      Result += "\\t";
      break;
    default:
      Result.push_back(Character);
      break;
    }
  }
  return Result;
}

std::string SerializeProjectJson(const Project::ProjectDescriptor &Project) {
  std::ostringstream Stream;
  Stream << "{"
         << "\"projectId\":\"" << EscapeJsonString(Project.Manifest.ProjectId)
         << "\",\"name\":\"" << EscapeJsonString(Project.Manifest.Name)
         << "\",\"slug\":\"" << EscapeJsonString(Project.Manifest.Slug)
         << "\",\"rootPath\":\""
         << EscapeJsonString(Project.Root.RootPath.string())
         << "\",\"contentDir\":\""
         << EscapeJsonString(Project.Root.ContentDir.string())
         << "\",\"scriptsDir\":\""
         << EscapeJsonString(Project.ScriptWorkspace.ScriptsDir.string())
         << "\",\"scriptProjectPath\":\""
         << EscapeJsonString(Project.ScriptWorkspace.ScriptProjectPath.string())
         << "\",\"scriptSolutionPath\":\""
         << EscapeJsonString(Project.ScriptWorkspace.ScriptSolutionPath.string())
         << "\",\"scriptAssemblyName\":\""
         << EscapeJsonString(Project.ScriptWorkspace.AssemblyName)
         << "\",\"scriptRootNamespace\":\""
         << EscapeJsonString(Project.ScriptWorkspace.RootNamespace)
         << "\",\"starterScriptPath\":\""
         << EscapeJsonString(Project.ScriptWorkspace.StarterScriptPath.string())
         << "\",\"starterScriptClassName\":\""
         << EscapeJsonString(Project.ScriptWorkspace.StarterScriptClassName)
         << "\",\"starterScriptQualifiedClassName\":\""
         << EscapeJsonString(
                Project.ScriptWorkspace.StarterScriptQualifiedClassName)
         << "\",\"engineContentDir\":\""
         << EscapeJsonString((std::filesystem::path(AXIOM_CONTENT_DIR) / "Engine").string())
         << "\",\"sceneFilePath\":\""
         << EscapeJsonString(Project.Root.SceneFilePath.string())
         << "\"}";
  return Stream.str();
}

std::string SerializeProjectList(
    const std::vector<Project::ProjectDescriptor> &Projects,
    const std::optional<Project::ProjectDescriptor> &ActiveProject) {
  std::ostringstream Stream;
  Stream << "{\"type\":\"projects\",\"activeProjectSlug\":";
  if (ActiveProject.has_value()) {
    Stream << "\"" << EscapeJsonString(ActiveProject->Manifest.Slug) << "\"";
  } else {
    Stream << "null";
  }
  Stream << ",\"projects\":[";
  for (size_t Index = 0; Index < Projects.size(); ++Index) {
    if (Index > 0) {
      Stream << ",";
    }
    Stream << SerializeProjectJson(Projects[Index]);
  }
  Stream << "]}";
  return Stream.str();
}

std::string SerializeCurrentProject(
    const std::optional<Project::ProjectDescriptor> &ActiveProject) {
  std::ostringstream Stream;
  Stream << "{\"type\":\"current_project\",\"project\":";
  if (ActiveProject.has_value()) {
    Stream << SerializeProjectJson(*ActiveProject);
  } else {
    Stream << "null";
  }
  Stream << "}";
  return Stream.str();
}

// Loads an image file, scales it to fit within MaxDim x MaxDim (preserving
// aspect ratio), and encodes as JPEG. Returns empty vector on any failure.
std::vector<uint8_t> MakeThumbnailJpeg(const std::filesystem::path &Path,
                                        int MaxDim = 128) {
  int W = 0, H = 0, Channels = 0;
  stbi_uc *Pixels =
      stbi_load(Path.string().c_str(), &W, &H, &Channels, STBI_rgb);
  if (!Pixels || W <= 0 || H <= 0) return {};

  // Compute thumbnail dimensions preserving aspect ratio.
  int ThumbW = W, ThumbH = H;
  if (W > MaxDim || H > MaxDim) {
    if (W >= H) {
      ThumbW = MaxDim;
      ThumbH = std::max(1, H * MaxDim / W);
    } else {
      ThumbH = MaxDim;
      ThumbW = std::max(1, W * MaxDim / H);
    }
  }

  // Nearest-neighbour downsample (fast, acceptable for small thumbnails).
  std::vector<uint8_t> Scaled(static_cast<size_t>(ThumbW * ThumbH * 3));
  for (int Y = 0; Y < ThumbH; ++Y) {
    for (int X = 0; X < ThumbW; ++X) {
      int SrcX = X * W / ThumbW;
      int SrcY = Y * H / ThumbH;
      const stbi_uc *Src = Pixels + (SrcY * W + SrcX) * 3;
      uint8_t *Dst = Scaled.data() + (Y * ThumbW + X) * 3;
      Dst[0] = Src[0];
      Dst[1] = Src[1];
      Dst[2] = Src[2];
    }
  }
  stbi_image_free(Pixels);

  std::vector<uint8_t> JpegBytes;
  stbi_write_jpg_to_func(
      [](void *Ctx, void *Data, int Size) {
        auto *Out = static_cast<std::vector<uint8_t> *>(Ctx);
        const uint8_t *Bytes = static_cast<const uint8_t *>(Data);
        Out->insert(Out->end(), Bytes, Bytes + Size);
      },
      &JpegBytes, ThumbW, ThumbH, 3, Scaled.data(), 85);
  return JpegBytes;
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
    : m_Host(Host),
      m_Options(Options),
      m_ProjectsRoot(Project::GetDefaultProjectsRoot()) {
  m_Host.SetTransportVideoEncoder(nullptr);
}

RemoteViewportServer::~RemoteViewportServer() { Stop(); }

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
  m_PresenceThread = std::thread([this]() { PresenceLoop(); });
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
  if (m_PresenceThread.joinable()) {
    m_PresenceThread.join();
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
  if (Frame.User.Value != 0u) {
    if (const HeadlessRenderViewState *RenderView =
            m_Host.FindRenderView(Frame.User);
        RenderView != nullptr && !RenderView->IsLocal) {
      if (RemoteClientSession *Client = FindClientSession(RenderView->ClientId);
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
  }
}

static constexpr int AwayThresholdSeconds = 10;
static constexpr int DisconnectThresholdSeconds = 30;
static constexpr int PresenceCheckIntervalMs = 2000;

void RemoteViewportServer::PresenceLoop() {
  while (!m_StopRequested.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(PresenceCheckIntervalMs));
    if (m_StopRequested.load()) {
      break;
    }

    const auto Now = std::chrono::steady_clock::now();
    std::vector<std::pair<SessionUserId, EditorUserPresenceState>> Transitions;

    {
      std::scoped_lock Lock(m_ClientMutex);
      for (const auto &[ClientId, Client] : m_RemoteClientsById) {
        const auto Elapsed =
            std::chrono::duration_cast<std::chrono::seconds>(Now - Client.LastActivity)
                .count();
        const EditorUserPresence *Presence =
            m_Host.GetHeadlessLayer().GetSession().FindPresence(Client.User);
        if (Presence == nullptr) {
          continue;
        }
        if (Elapsed >= DisconnectThresholdSeconds &&
            Presence->State == EditorUserPresenceState::Away) {
          Transitions.emplace_back(Client.User, EditorUserPresenceState::Disconnected);
        } else if (Elapsed >= AwayThresholdSeconds &&
                   Presence->State == EditorUserPresenceState::Connected) {
          Transitions.emplace_back(Client.User, EditorUserPresenceState::Away);
        }
      }
    }

    for (const auto &[User, State] : Transitions) {
      m_Host.GetHeadlessLayer().GetSession().SetPresenceState(User, State);
      if (State == EditorUserPresenceState::Disconnected) {
        m_Host.GetHeadlessLayer().GetSession().ReleaseAllLocksForUser(User);
      }
    }
  }
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
  if (Route == "/projects/create") {
    return HandleCreateProjectRequest(ClientSocketValue, Body);
  }
  if (Route == "/projects/open") {
    return HandleOpenProjectRequest(ClientSocketValue, Body);
  }
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
  if (Route == "/assets/upload") {
    return HandleAssetUploadRequest(ClientSocketValue, Path, HeaderBlock, Body);
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
    m_Host.SetRemoteViewMode(*User, Command->ViewMode);
    break;
  case HeadlessCommandType::SetLookActive:
  case HeadlessCommandType::SetViewportCameraPose:
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
  case HeadlessCommandType::SetMeshAsset:
  case HeadlessCommandType::SetLightProperties:
  case HeadlessCommandType::SetMaterialProperties:
  case HeadlessCommandType::SetMaterialTexture:
    m_Host.SubmitRemoteCommand(*User, Command->EditorPayload);
    break;
  case HeadlessCommandType::DropMesh:
    HandleMeshDropCommand(*User, *Command);
    break;
  case HeadlessCommandType::DropTexture: {
    HandleTextureDropCommand(*User, *Command);
    break;
  }
  case HeadlessCommandType::GizmoHover:
  case HeadlessCommandType::GizmoDragStart:
  case HeadlessCommandType::GizmoDragUpdate:
  case HeadlessCommandType::GizmoDragEnd:
  case HeadlessCommandType::SetGizmoMode:
  case HeadlessCommandType::SetGridSnap:
    break;
  case HeadlessCommandType::Heartbeat:
  case HeadlessCommandType::ListAssets:
  case HeadlessCommandType::GetSchema:
  case HeadlessCommandType::SetProperty:
  case HeadlessCommandType::SaveScene:
  case HeadlessCommandType::ReloadScripts:
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

bool RemoteViewportServer::HandleCreateProjectRequest(
    uintptr_t ClientSocketValue, std::string_view Body) {
  const SocketHandle ClientSocket = ToSocket(ClientSocketValue);
  const auto ProjectName = ExtractJsonStringField(Body, "name");
  if (!ProjectName.has_value() || ProjectName->empty()) {
    const std::string Response = JsonResponse(
        "400 Bad Request",
        SerializeError("Missing required 'name' field."));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  std::string FailureReason;
  const auto Created = Project::CreateProjectScaffold(m_ProjectsRoot, *ProjectName,
                                                      &FailureReason);
  if (!Created.has_value()) {
    const std::string Response = JsonResponse(
        "400 Bad Request", SerializeError(FailureReason));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  {
    std::scoped_lock Lock(m_ProjectMutex);
    m_ActiveProject = *Created;
  }

  FailureReason.clear();
  if (!LoadActiveProjectIntoSession(&FailureReason)) {
    const std::string Response = JsonResponse(
        "500 Internal Server Error", SerializeError(FailureReason));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  const std::string Response =
      JsonResponse("201 Created", SerializeCurrentProject(Created));
  SendAll(ClientSocket, Response.data(), Response.size());
  return false;
}

bool RemoteViewportServer::HandleOpenProjectRequest(uintptr_t ClientSocketValue,
                                                    std::string_view Body) {
  const SocketHandle ClientSocket = ToSocket(ClientSocketValue);
  const auto ProjectSlug = ExtractJsonStringField(Body, "slug");
  if (!ProjectSlug.has_value() || ProjectSlug->empty()) {
    const std::string Response = JsonResponse(
        "400 Bad Request",
        SerializeError("Missing required 'slug' field."));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  const auto Opened = SetActiveProjectBySlug(*ProjectSlug);
  if (!Opened.has_value()) {
    const std::string Response = JsonResponse(
        "404 Not Found",
        SerializeError("Project was not found in the managed projects directory."));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  std::string FailureReason;
  if (!LoadActiveProjectIntoSession(&FailureReason)) {
    const std::string Response = JsonResponse(
        "500 Internal Server Error", SerializeError(FailureReason));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  const std::string Response =
      JsonResponse("200 OK", SerializeCurrentProject(Opened));
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
  if (Route == "/projects") {
    const auto Projects = ListProjects();
    const auto ActiveProject = GetActiveProject();
    const std::string Response =
        JsonResponse("200 OK", SerializeProjectList(Projects, ActiveProject));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }
  if (Route == "/projects/current") {
    const auto ActiveProject = GetActiveProject();
    const std::string Response =
        JsonResponse("200 OK", SerializeCurrentProject(ActiveProject));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }
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
  if (Route == "/assets/thumbnail") {
    const auto RelPath = GetQueryParam(Path, "path");
    if (!RelPath.has_value() || RelPath->empty()) {
      const std::string Response =
          JsonResponse("400 Bad Request", SerializeError("Missing 'path' query parameter."));
      SendAll(ClientSocket, Response.data(), Response.size());
      return false;
    }
    // Reject any path that tries to escape the content directory.
    if (RelPath->find("..") != std::string::npos) {
      const std::string Response =
          JsonResponse("400 Bad Request", SerializeError("Invalid path."));
      SendAll(ClientSocket, Response.data(), Response.size());
      return false;
    }
    const auto FullPath = ResolveVisibleAssetPath(*RelPath);
    if (!FullPath.has_value()) {
      const std::string Response =
          JsonResponse("400 Bad Request", SerializeError("Invalid path."));
      SendAll(ClientSocket, Response.data(), Response.size());
      return false;
    }
    const std::vector<uint8_t> Jpeg = MakeThumbnailJpeg(*FullPath);
    if (Jpeg.empty()) {
      const std::string Response =
          JsonResponse("404 Not Found", SerializeError("Could not load thumbnail."));
      SendAll(ClientSocket, Response.data(), Response.size());
      return false;
    }
    const std::string_view JpegView(reinterpret_cast<const char *>(Jpeg.data()),
                                    Jpeg.size());
    const std::string Response =
        BuildHttpResponse("200 OK", "image/jpeg", JpegView);
    SendAll(ClientSocket, Response.data(), Response.size());
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
      EditorSession &DisconnectSession = m_Host.GetHeadlessLayer().GetSession();
      DisconnectSession.ReleaseAllLocksForUser(*DisconnectedUser);
      DisconnectSession.SetPresenceState(*DisconnectedUser,
                                         EditorUserPresenceState::Disconnected);
    }
    m_Host.RemoveRemoteRenderView(*ClientId);
  }
  m_Host.FocusLocalRenderView();
  const WebRtcSessionStatus Status = Client->WebRtcSession->GetStatus();
  const std::string Payload = SerializeWebRtcStatus(
      Status.Enabled, Status.Available, Status.SignalingState,
      Status.ConnectionState, Status.Detail, Status.SessionId,
      Status.PendingLocalIceCandidateCount, Status.Video);
  const std::string Response = JsonResponse("200 OK", Payload);
  SendAll(ClientSocket, Response.data(), Response.size());
  return false;
}

bool RemoteViewportServer::HandleAssetUploadRequest(
    uintptr_t ClientSocketValue, std::string_view Path,
    std::string_view HeaderBlock, std::string_view Body) {
  const SocketHandle ClientSocket = ToSocket(ClientSocketValue);

  // Parse boundary from Content-Type header.
  const auto ContentType = FindHeaderValue(HeaderBlock, "Content-Type");
  if (!ContentType.has_value() ||
      ContentType->find("multipart/form-data") == std::string::npos) {
    const std::string Response = JsonResponse(
        "400 Bad Request",
        SerializeError("Expected multipart/form-data Content-Type."));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }
  const auto BoundaryPos = ContentType->find("boundary=");
  if (BoundaryPos == std::string::npos) {
    const std::string Response =
        JsonResponse("400 Bad Request", SerializeError("Missing boundary."));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }
  const std::string Boundary =
      "--" + ContentType->substr(BoundaryPos + 9 /* len("boundary=") */);

  // Optional target subdirectory from ?dir= query parameter.
  const std::string TargetDir = GetQueryParam(Path, "dir").value_or("");

  const auto ContentRoot = GetActiveContentDir();
  const Assets::LocalAssetSource ContentSource{ContentRoot};

  // Resolve and validate destination directory.
  std::filesystem::path DestDir;
  if (TargetDir.empty()) {
    DestDir = ContentRoot;
  } else {
    const std::filesystem::path TargetDirPath{TargetDir};
    // Prevent path traversal: reject any component that starts with ".."
    bool Unsafe = false;
    for (const auto &Part : TargetDirPath) {
      if (Part.string().find("..") != std::string::npos) {
        Unsafe = true;
        break;
      }
    }
    if (Unsafe) {
      const std::string Response = JsonResponse(
          "400 Bad Request", SerializeError("Invalid target directory."));
      SendAll(ClientSocket, Response.data(), Response.size());
      return false;
    }
    if (TargetDirPath.begin() != TargetDirPath.end() &&
        (*TargetDirPath.begin()).string() == "Engine") {
      const std::string Response = JsonResponse(
          "400 Bad Request",
          SerializeError("Engine content is read-only and cannot be modified by project uploads."));
      SendAll(ClientSocket, Response.data(), Response.size());
      return false;
    }
    DestDir = ContentRoot / TargetDirPath;
  }

  // Split multipart body into parts.
  static constexpr std::string_view kContentDisposition = "Content-Disposition:";
  std::vector<std::string> Saved;
  std::string_view Remaining{Body};

  while (true) {
    // Find boundary
    const auto BPos = Remaining.find(Boundary);
    if (BPos == std::string_view::npos) break;
    Remaining.remove_prefix(BPos + Boundary.size());
    // Skip "\r\n" after boundary, or stop on "--" (final boundary)
    if (Remaining.starts_with("--")) break;
    if (Remaining.starts_with("\r\n")) Remaining.remove_prefix(2);

    // Find end of part headers (blank line)
    const auto HeaderEnd = Remaining.find("\r\n\r\n");
    if (HeaderEnd == std::string_view::npos) break;
    const std::string_view PartHeaders = Remaining.substr(0, HeaderEnd);
    Remaining.remove_prefix(HeaderEnd + 4);

    // Find filename in Content-Disposition
    const auto CDPos = PartHeaders.find(kContentDisposition);
    if (CDPos == std::string_view::npos) continue;
    const auto FnPos = PartHeaders.find("filename=\"", CDPos);
    if (FnPos == std::string_view::npos) continue;
    const auto FnStart = FnPos + 10;
    const auto FnEnd = PartHeaders.find('"', FnStart);
    if (FnEnd == std::string_view::npos) continue;
    const std::string Filename{PartHeaders.substr(FnStart, FnEnd - FnStart)};
    if (Filename.empty()) continue;

    // Find part body (ends at next boundary)
    const auto BodyEnd = Remaining.find(Boundary);
    if (BodyEnd == std::string_view::npos) break;
    // Strip trailing \r\n before boundary
    const size_t PartBodyLen = BodyEnd >= 2 ? BodyEnd - 2 : BodyEnd;
    const std::string_view PartBody = Remaining.substr(0, PartBodyLen);

    // Validate extension
    const std::filesystem::path FilePath{Filename};
    const std::string Ext = [&] {
      auto E = FilePath.extension().string();
      for (auto &C : E) C = static_cast<char>(std::tolower(static_cast<unsigned char>(C)));
      return E;
    }();
    static constexpr std::string_view kAllowed[] = {
        ".glb", ".gltf", ".fbx", ".obj", ".png", ".jpg", ".jpeg"};
    bool Allowed = false;
    for (const auto &A : kAllowed) {
      if (Ext == A) { Allowed = true; break; }
    }
    if (!Allowed) {
      std::cerr << "[AssetUpload] rejected '" << Filename
                << "': unsupported extension\n";
      continue;
    }

    // Write file to destination, creating directories as needed.
    std::error_code Ec;
    std::filesystem::create_directories(DestDir, Ec);
    const std::filesystem::path OutPath = DestDir / FilePath.filename();
    std::ofstream OutFile(OutPath, std::ios::binary);
    if (!OutFile.is_open()) {
      std::cerr << "[AssetUpload] could not open '" << OutPath.string()
                << "' for writing\n";
      continue;
    }
    OutFile.write(PartBody.data(), static_cast<std::streamsize>(PartBody.size()));
    OutFile.close();
    std::cerr << "[AssetUpload] saved '" << OutPath.string() << "'\n";

    // Build content-relative path for the response.
    const auto Rel = std::filesystem::relative(OutPath,
                                               ContentRoot, Ec);
    if (!Ec) Saved.push_back(Rel.string());
  }

  // Broadcast updated asset list to all WebSocket clients.
  {
    BroadcastTextMessage(SerializeAssetList(CollectVisibleAssets()));
  }

  // Build JSON response with saved paths.
  std::ostringstream Out;
  Out << "{\"type\":\"assets_uploaded\",\"files\":[";
  for (size_t i = 0; i < Saved.size(); ++i) {
    if (i > 0) Out << ",";
    Out << "\"" << EscapeJson(Saved[i]) << "\"";
  }
  Out << "]}";
  const std::string Response = JsonResponse("200 OK", Out.str());
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
  m_Host.EnsureRemoteRenderView(ResolvedSession->ClientId, ResolvedSession->User);
  return {.Session = ResolvedSession, .ResumedExisting = ResumedExisting};
}

void RemoteViewportServer::TouchClientSession(const std::string &ClientId) {
  std::scoped_lock Lock(m_ClientMutex);
  const auto It = m_RemoteClientsById.find(ClientId);
  if (It != m_RemoteClientsById.end()) {
    It->second.LastActivity = std::chrono::steady_clock::now();
  }
  m_Host.FocusRemoteRenderView(ClientId);
}

std::vector<Project::ProjectDescriptor> RemoteViewportServer::ListProjects() const {
  return Project::DiscoverProjects(m_ProjectsRoot);
}

std::optional<Project::ProjectDescriptor>
RemoteViewportServer::GetActiveProject() const {
  std::scoped_lock Lock(m_ProjectMutex);
  return m_ActiveProject;
}

std::optional<Project::ProjectDescriptor>
RemoteViewportServer::SetActiveProjectBySlug(std::string_view ProjectSlug) {
  const auto Opened = Project::OpenProjectBySlug(m_ProjectsRoot, ProjectSlug);
  if (!Opened.has_value()) {
    return std::nullopt;
  }

  {
    std::scoped_lock Lock(m_ProjectMutex);
    m_ActiveProject = *Opened;
  }
  return Opened;
}

std::filesystem::path RemoteViewportServer::GetActiveContentDir() const {
  if (const auto ActiveProject = GetActiveProject(); ActiveProject.has_value()) {
    return ActiveProject->Root.ContentDir;
  }
  return std::filesystem::path(AXIOM_CONTENT_DIR);
}

std::filesystem::path RemoteViewportServer::GetEngineContentDir() const {
  return std::filesystem::path(AXIOM_CONTENT_DIR) / "Engine";
}

bool RemoteViewportServer::LoadActiveProjectIntoSession(
    std::string *FailureReason) {
  if (m_Host.LoadStartupSceneIntoSession(GetActiveContentDir())) {
    return true;
  }

  if (FailureReason != nullptr) {
    *FailureReason =
        "Failed to load the active project's startup scene into the session.";
  }
  return false;
}

std::vector<Assets::AssetDescriptor>
RemoteViewportServer::CollectVisibleAssets() const {
  std::vector<Assets::AssetDescriptor> Assets;

  const Assets::LocalAssetSource ProjectSource{GetActiveContentDir()};
  Assets = ProjectSource.List();

  const Assets::LocalAssetSource EngineSource{GetEngineContentDir()};
  for (auto EngineAsset : EngineSource.List()) {
    EngineAsset.RelativePath =
        (std::filesystem::path("Engine") / EngineAsset.RelativePath)
            .generic_string();
    EngineAsset.Name = std::filesystem::path(EngineAsset.RelativePath).stem().string();
    EngineAsset.Id = Assets::AssetIdFromRelativePath(EngineAsset.RelativePath);
    Assets.push_back(std::move(EngineAsset));
  }

  std::sort(Assets.begin(), Assets.end(),
            [](const Assets::AssetDescriptor &Left,
               const Assets::AssetDescriptor &Right) {
              return Left.RelativePath < Right.RelativePath;
            });
  return Assets;
}

std::optional<std::filesystem::path>
RemoteViewportServer::ResolveVisibleAssetPath(std::string_view RelativePath) const {
  if (RelativePath.empty()) {
    return std::nullopt;
  }

  const std::filesystem::path Relative{std::string(RelativePath)};
  for (const auto &Part : Relative) {
    if (Part == "..") {
      return std::nullopt;
    }
  }

  if (!Relative.empty() && *Relative.begin() == "Engine") {
    std::filesystem::path EngineRelative;
    auto It = Relative.begin();
    ++It;
    for (; It != Relative.end(); ++It) {
      EngineRelative /= *It;
    }
    return GetEngineContentDir() / EngineRelative;
  }

  return GetActiveContentDir() / Relative;
}

void RemoteViewportServer::HandleClientEncodedVideoPacket(
    std::string_view ClientId, const EncodedVideoPacket &Packet) {
  if (RemoteClientSession *Client = FindClientSession(ClientId);
      Client != nullptr && Client->WebRtcSession != nullptr) {
    Client->WebRtcSession->OnEncodedVideoPacket(Packet);
  }
}

void RemoteViewportServer::HandleTextureDropCommand(
    SessionUserId User, const HeadlessCommand &Command) {
  if (Command.TextureAssetPath.empty()) {
    return;
  }

  const EditorSession &Session = m_Host.GetHeadlessLayer().GetSession();
  const EditorViewportState *Viewport = Session.FindViewport(User);
  if (Viewport == nullptr) {
    return;
  }

  const std::string HitId = HitTestMeshes(
      Viewport->Camera, m_Options.Width, m_Options.Height,
      Command.MousePosition, Session.GetState().Scene.MeshInstances);
  if (HitId.empty()) {
    return;
  }

  m_Host.SubmitRemoteCommand(User, EditorCommand{SetMaterialTextureCommand{
                                       .ObjectId = HitId,
                                       .TextureAssetPath = Command.TextureAssetPath,
                                   }});
}

void RemoteViewportServer::HandleMeshDropCommand(SessionUserId User,
                                                 const HeadlessCommand &Command) {
  if (Command.MeshAssetPath.empty()) {
    return;
  }

  const EditorSession &Session = m_Host.GetHeadlessLayer().GetSession();
  const EditorViewportState *Viewport = Session.FindViewport(User);
  if (Viewport == nullptr) {
    return;
  }

  const glm::vec3 SpawnLocation = ResolveViewportDropPosition(
      Viewport->Camera, m_Options.Width, m_Options.Height, Command.MousePosition,
      Session.GetState().Scene.MeshInstances);

  m_Host.SubmitRemoteCommand(User, EditorCommand{CreateMeshObjectCommand{
                                       .AssetPath = Command.MeshAssetPath,
                                       .Location = SpawnLocation,
                                       .RotationDegrees = glm::vec3(0.0f),
                                       .Scale = glm::vec3(1.0f),
                                   }});
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
    if (!HandleWebSocketMessage(ClientSocketValue, TextPayload)) {
      SendTextMessage(ClientSocketValue,
                      SerializeError("Invalid WebSocket command payload."));
    }
  }

  RemoveWebSocketClient(ClientSocketValue);
}

bool RemoteViewportServer::HandleWebSocketMessage(uintptr_t ClientSocketValue,
                                                  std::string_view Payload) {
  std::string Error;
  const auto Command = ParseRemoteViewportCommand(Payload, Error);
  if (!Command.has_value()) {
    return false;
  }

  switch (Command->Type) {
  case HeadlessCommandType::SetViewMode:
    m_Host.SetRemoteViewMode(Command->ViewMode);
    return true;
  case HeadlessCommandType::DropMesh:
    HandleMeshDropCommand(m_Host.GetHeadlessLayer().GetLocalUserId(), *Command);
    return true;
  case HeadlessCommandType::DropTexture:
    HandleTextureDropCommand(m_Host.GetHeadlessLayer().GetLocalUserId(), *Command);
    return true;
  case HeadlessCommandType::SetLookActive:
  case HeadlessCommandType::SetViewportCameraPose:
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
  case HeadlessCommandType::SetMeshAsset:
  case HeadlessCommandType::SetLightProperties:
  case HeadlessCommandType::SetMaterialProperties:
  case HeadlessCommandType::SetMaterialTexture:
  case HeadlessCommandType::ReloadScripts:
  case HeadlessCommandType::UpdateViewportCamera:
  case HeadlessCommandType::GizmoHover:
  case HeadlessCommandType::GizmoDragStart:
  case HeadlessCommandType::GizmoDragUpdate:
  case HeadlessCommandType::GizmoDragEnd:
  case HeadlessCommandType::SetGizmoMode:
  case HeadlessCommandType::SetGridSnap:
  case HeadlessCommandType::Heartbeat:
    return false;
  case HeadlessCommandType::ListAssets: {
    SendTextMessage(ClientSocketValue, SerializeAssetList(CollectVisibleAssets()));
    return true;
  }
  case HeadlessCommandType::GetSchema: {
    const auto &DetailsById =
        m_Host.GetHeadlessLayer().GetSession().GetState().Scene.ObjectDetailsById;
    const auto It = DetailsById.find(Command->ObjectId);
    if (It != DetailsById.end()) {
      SendTextMessage(ClientSocketValue, SerializeObjectSchema(It->second));
    }
    return true;
  }
  case HeadlessCommandType::SetProperty:
    return false;
  case HeadlessCommandType::SaveScene: {
    const Assets::LocalAssetSource ContentDir{GetActiveContentDir()};
    const auto ScenePath = ContentDir.ResolveRelative("scene.json");
    const bool Ok = Assets::SaveSceneToFile(
        ScenePath,
        m_Host.GetHeadlessLayer().GetSession().GetState().Scene);
    SendTextMessage(ClientSocketValue, SerializeSaveResult(Ok));
    return true;
  }
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
    m_Host.SetRemoteViewMode(Client->User, Command->ViewMode);
    return true;
  case HeadlessCommandType::SetLookActive:
  case HeadlessCommandType::SetViewportCameraPose:
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
  case HeadlessCommandType::SetMeshAsset:
  case HeadlessCommandType::SetLightProperties:
  case HeadlessCommandType::SetMaterialProperties:
  case HeadlessCommandType::SetMaterialTexture:
    m_Host.SubmitRemoteCommand(Client->User, Command->EditorPayload);
    return true;
  case HeadlessCommandType::DropMesh:
    HandleMeshDropCommand(Client->User, *Command);
    return true;
  case HeadlessCommandType::ReloadScripts: {
    m_Host.ReloadUserScripts();
    if (Client->WebRtcSession != nullptr) {
      Client->WebRtcSession->SendReliableMessage(
          "{\"type\":\"scripts_reloaded\"}");
    }
    return true;
  }
  case HeadlessCommandType::Heartbeat: {
    const EditorUserPresence *Presence =
        m_Host.GetHeadlessLayer().GetSession().FindPresence(Client->User);
    if (Presence != nullptr && Presence->State == EditorUserPresenceState::Away) {
      m_Host.GetHeadlessLayer().GetSession().SetPresenceState(
          Client->User, EditorUserPresenceState::Connected);
    }
    return true;
  }
  case HeadlessCommandType::ListAssets: {
    if (Client->WebRtcSession != nullptr) {
      Client->WebRtcSession->SendReliableMessage(
          SerializeAssetList(CollectVisibleAssets()));
    }
    return true;
  }
  case HeadlessCommandType::GetSchema: {
    const auto &DetailsById =
        m_Host.GetHeadlessLayer().GetSession().GetState().Scene.ObjectDetailsById;
    const auto It = DetailsById.find(Command->ObjectId);
    if (It != DetailsById.end() && Client->WebRtcSession != nullptr) {
      Client->WebRtcSession->SendReliableMessage(
          SerializeObjectSchema(It->second));
    }
    return true;
  }
  case HeadlessCommandType::SaveScene: {
    const Assets::LocalAssetSource ContentDir{GetActiveContentDir()};
    const auto ScenePath = ContentDir.ResolveRelative("scene.json");
    const bool Ok = Assets::SaveSceneToFile(
        ScenePath,
        m_Host.GetHeadlessLayer().GetSession().GetState().Scene);
    if (Client->WebRtcSession != nullptr) {
      Client->WebRtcSession->SendReliableMessage(SerializeSaveResult(Ok));
    }
    return true;
  }
  case HeadlessCommandType::SetProperty: {
    if (!Command->PropertyVal.has_value()) {
      return false;
    }
    const auto &Name = Command->PropertyName;
    const auto &Val = *Command->PropertyVal;
    const auto &ObjId = Command->ObjectId;

    if (Name == "displayName") {
      if (const auto *S = std::get_if<std::string>(&Val)) {
        m_Host.SubmitRemoteCommand(
            Client->User,
            EditorCommand{RenameObjectCommand{.ObjectId = ObjId, .DisplayName = *S}});
        return true;
      }
    } else if (Name == "visible") {
      if (const auto *B = std::get_if<bool>(&Val)) {
        m_Host.SubmitRemoteCommand(
            Client->User,
            EditorCommand{SetObjectVisibilityCommand{.ObjectId = ObjId, .Visible = *B}});
        return true;
      }
    } else if (Name == "scriptClass") {
      if (const auto *S = std::get_if<std::string>(&Val)) {
        if (S->empty()) {
          m_Host.SubmitRemoteCommand(
              Client->User,
              EditorCommand{DetachScriptCommand{.ObjectId = ObjId}});
        } else {
          m_Host.SubmitRemoteCommand(
              Client->User,
              EditorCommand{AttachScriptCommand{.ObjectId = ObjId,
                                               .ScriptClassName = *S}});
        }
        return true;
      }
    } else if (Name == "location" || Name == "rotationDegrees" || Name == "scale") {
      if (const auto *V = std::get_if<glm::vec3>(&Val)) {
        const auto &DetailsById =
            m_Host.GetHeadlessLayer().GetSession().GetState().Scene.ObjectDetailsById;
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
        if (Name == "location")        Cmd.Location        = *V;
        else if (Name == "rotationDegrees") Cmd.RotationDegrees = *V;
        else                           Cmd.Scale           = *V;
        m_Host.SubmitRemoteCommand(Client->User, EditorCommand{Cmd});
        return true;
      }
    }
    return false;
  }
  case HeadlessCommandType::SetGizmoMode:
    Client->CurrentGizmoMode = Command->Mode;
    m_Host.GetHeadlessLayer().SetGizmoMode(Client->User, Command->Mode);
    return true;
  case HeadlessCommandType::SetGridSnap: {
    Client->GridSnap.Enabled = Command->Enabled;
    Client->GridSnap.TranslationStep =
        std::max(kMinimumScale, Command->TranslationStep);
    Client->GridSnap.RotationStepDegrees =
        std::max(0.001f, Command->RotationStepDegrees);
    Client->GridSnap.ScaleStep = std::max(kMinimumScale, Command->ScaleStep);
    return true;
  }
  case HeadlessCommandType::GizmoHover: {
    if (Client->GizmoDrag.has_value()) {
      return true;
    }
    const EditorSession &Session =
        m_Host.GetHeadlessLayer().GetSession();
    const EditorViewportState *Viewport =
        Session.FindViewport(Client->User);
    const EditorObjectDetails *Selected =
        Session.FindSelectedObjectDetails(Client->User);
    const auto *HoverTD = (Selected != nullptr && Selected->SupportsTransform)
        ? (Selected->WorldTransform.has_value() ? &*Selected->WorldTransform
               : (Selected->Transform.has_value() ? &*Selected->Transform : nullptr))
        : nullptr;
    if (Viewport != nullptr && HoverTD != nullptr) {
      const float GizmoScale = ComputeGizmoScale(
          Viewport->Camera, HoverTD->Location,
          m_Options.Width, m_Options.Height);
      const int Axis =
          (Client->CurrentGizmoMode == GizmoMode::Rotate)
              ? HitTestGizmoRings(Viewport->Camera, m_Options.Width,
                                  m_Options.Height, Command->MousePosition,
                                  HoverTD->Location, GizmoScale)
              : HitTestGizmoAxes(Viewport->Camera, m_Options.Width,
                                 m_Options.Height, Command->MousePosition,
                                 HoverTD->Location, GizmoScale);
      m_Host.GetHeadlessLayer().SetGizmoHoveredAxis(Client->User, Axis);
    } else {
      m_Host.GetHeadlessLayer().SetGizmoHoveredAxis(Client->User, -1);
    }
    return true;
  }
  case HeadlessCommandType::GizmoDragStart: {
    if (Client->GizmoDrag.has_value()) {
      return true;
    }
    EditorSession &Session =
        m_Host.GetHeadlessLayer().GetSession();
    const EditorViewportState *Viewport =
        Session.FindViewport(Client->User);
    if (Viewport == nullptr) {
      return true;
    }
    const EditorObjectDetails *Selected =
        Session.FindSelectedObjectDetails(Client->User);
    const auto *DragTD = (Selected != nullptr && Selected->SupportsTransform)
        ? (Selected->WorldTransform.has_value() ? &*Selected->WorldTransform
               : (Selected->Transform.has_value() ? &*Selected->Transform : nullptr))
        : nullptr;

    if (DragTD != nullptr) {
      const glm::vec3 &ObjPos = DragTD->Location;
      const float GizmoScale = ComputeGizmoScale(
          Viewport->Camera, ObjPos, m_Options.Width, m_Options.Height);
      if (Client->CurrentGizmoMode == GizmoMode::Rotate) {
        auto DragState = BeginGizmoRotateDrag(
            Viewport->Camera, m_Options.Width, m_Options.Height,
            Command->MousePosition, ObjPos, GizmoScale, ObjPos);
        if (DragState.has_value()) {
          Client->GizmoDrag = ActiveGizmoDrag{
              .Math = *DragState,
              .ObjectId = Selected->ObjectId,
              .StartRotDeg = DragTD->RotationDegrees,
              .StartScale = DragTD->Scale,
              .Mode = GizmoMode::Rotate,
              .GizmoScaleAtDragStart = GizmoScale,
          };
          Session.AcquireLock(Selected->ObjectId, Client->User);
          m_Host.GetHeadlessLayer().SetGizmoHoveredAxis(Client->User, DragState->Axis);
          return true;
        }
      } else {
        auto DragState = BeginGizmoDrag(
            Viewport->Camera, m_Options.Width, m_Options.Height,
            Command->MousePosition, ObjPos, GizmoScale, ObjPos);
        if (DragState.has_value()) {
          Client->GizmoDrag = ActiveGizmoDrag{
              .Math = *DragState,
              .ObjectId = Selected->ObjectId,
              .StartRotDeg = DragTD->RotationDegrees,
              .StartScale = DragTD->Scale,
              .Mode = Client->CurrentGizmoMode,
              .GizmoScaleAtDragStart = GizmoScale,
          };
          Session.AcquireLock(Selected->ObjectId, Client->User);
          m_Host.GetHeadlessLayer().SetGizmoHoveredAxis(Client->User, DragState->Axis);
          return true;
        }
      }
    }

    // No gizmo hit — fall back to viewport object picking.
    const auto Hit = ResolveViewportSelectionHit(
        Viewport->Camera, m_Options.Width, m_Options.Height,
        Command->MousePosition, Session.GetState().Scene.MeshInstances,
        m_Host.GetHeadlessLayer().BuildLightBillboards());
    if (Hit.has_value() && !Hit->ObjectId.empty()) {
      m_Host.SubmitRemoteCommand(Client->User,
          EditorCommand{SelectObjectCommand{.ObjectId = Hit->ObjectId}});
    }
    return true;
  }
  case HeadlessCommandType::DropTexture: {
    HandleTextureDropCommand(Client->User, *Command);
    return true;
  }
  case HeadlessCommandType::GizmoDragUpdate: {
    if (!Client->GizmoDrag.has_value()) {
      return true;
    }
    const EditorSession &Session =
        m_Host.GetHeadlessLayer().GetSession();
    const EditorViewportState *Viewport =
        Session.FindViewport(Client->User);
    if (Viewport == nullptr) {
      return true;
    }
    const ActiveGizmoDrag &Drag = *Client->GizmoDrag;
    glm::vec3 Location = Drag.Math.ObjectStartPos;
    glm::vec3 RotDeg = Drag.StartRotDeg;
    glm::vec3 Scale = Drag.StartScale;
    if (Drag.Mode == GizmoMode::Translate) {
      Location = UpdateGizmoDrag(Drag.Math, Viewport->Camera,
                                 m_Options.Width, m_Options.Height,
                                 Command->MousePosition.x, Command->MousePosition.y);
    } else if (Drag.Mode == GizmoMode::Scale) {
      const glm::vec3 NewPosTmp =
          UpdateGizmoDrag(Drag.Math, Viewport->Camera, m_Options.Width,
                          m_Options.Height, Command->MousePosition.x,
                          Command->MousePosition.y);
      const float DeltaT =
          glm::dot(NewPosTmp - Drag.Math.ObjectStartPos, Drag.Math.AxisDir);
      const float Factor = std::max(
          0.001f, 1.0f + DeltaT / std::max(0.001f, Drag.GizmoScaleAtDragStart));
      Scale[Drag.Math.Axis] = Drag.StartScale[Drag.Math.Axis] * Factor;
    } else {
      const float DeltaDeg = UpdateGizmoRotateDrag(
          Drag.Math, Viewport->Camera, m_Options.Width, m_Options.Height,
          Command->MousePosition.x, Command->MousePosition.y);
      RotDeg[Drag.Math.Axis] = Drag.StartRotDeg[Drag.Math.Axis] + DeltaDeg;
    }
    ApplyGridSnap(Client->GridSnap.Enabled, Client->GridSnap.TranslationStep,
                  Client->GridSnap.RotationStepDegrees, Client->GridSnap.ScaleStep,
                  Drag.Mode, Drag.Math.Axis, Location, RotDeg, Scale);
    EditorCommand Cmd;
    Cmd.Payload = SetTransformCommand{
        .ObjectId = Drag.ObjectId,
        .Location = Location,
        .RotationDegrees = RotDeg,
        .Scale = Scale,
    };
    m_Host.SubmitRemoteCommand(Client->User, Cmd);
    return true;
  }
  case HeadlessCommandType::GizmoDragEnd: {
    if (!Client->GizmoDrag.has_value()) {
      return true;
    }
    EditorSession &Session =
        m_Host.GetHeadlessLayer().GetSession();
    const EditorViewportState *Viewport =
        Session.FindViewport(Client->User);
    if (Viewport != nullptr) {
      const ActiveGizmoDrag &Drag = *Client->GizmoDrag;
      glm::vec3 Location = Drag.Math.ObjectStartPos;
      glm::vec3 RotDeg = Drag.StartRotDeg;
      glm::vec3 Scale = Drag.StartScale;
      if (Drag.Mode == GizmoMode::Translate) {
        Location = UpdateGizmoDrag(Drag.Math, Viewport->Camera,
                                   m_Options.Width, m_Options.Height,
                                   Command->MousePosition.x, Command->MousePosition.y);
      } else if (Drag.Mode == GizmoMode::Scale) {
        const glm::vec3 NewPosTmp =
            UpdateGizmoDrag(Drag.Math, Viewport->Camera, m_Options.Width,
                            m_Options.Height, Command->MousePosition.x,
                            Command->MousePosition.y);
        const float DeltaT =
            glm::dot(NewPosTmp - Drag.Math.ObjectStartPos, Drag.Math.AxisDir);
        const float Factor = std::max(
            0.001f, 1.0f + DeltaT / std::max(0.001f, Drag.GizmoScaleAtDragStart));
        Scale[Drag.Math.Axis] = Drag.StartScale[Drag.Math.Axis] * Factor;
      } else {
        const float DeltaDeg = UpdateGizmoRotateDrag(
            Drag.Math, Viewport->Camera, m_Options.Width, m_Options.Height,
            Command->MousePosition.x, Command->MousePosition.y);
        RotDeg[Drag.Math.Axis] = Drag.StartRotDeg[Drag.Math.Axis] + DeltaDeg;
      }
      ApplyGridSnap(Client->GridSnap.Enabled, Client->GridSnap.TranslationStep,
                    Client->GridSnap.RotationStepDegrees, Client->GridSnap.ScaleStep,
                    Drag.Mode, Drag.Math.Axis, Location, RotDeg, Scale);
      EditorCommand Cmd;
      Cmd.Payload = SetTransformCommand{
          .ObjectId = Drag.ObjectId,
          .Location = Location,
          .RotationDegrees = RotDeg,
          .Scale = Scale,
      };
      m_Host.SubmitRemoteCommand(Client->User, Cmd);
    }
    const std::string DragObjectId = Client->GizmoDrag->ObjectId;
    Client->GizmoDrag.reset();
    Session.ReleaseLock(DragObjectId, Client->User);
    m_Host.GetHeadlessLayer().SetGizmoHoveredAxis(Client->User, -1);
    return true;
  }
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
