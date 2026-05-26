#include "RemoteViewportServer.h"

#include <HAL/Socket.h>

#include <Core/Platform.h>
#include <Project/ProjectSystem.h>

#ifndef AXIOM_CONTENT_DIR
#define AXIOM_CONTENT_DIR "Content"
#endif

#include "GizmoHitTest.h"
#include "HeadlessCommandProtocol.h"
#include <App.h>
#include <HttpParser.h>
#include <Loop.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
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
#include <condition_variable>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <regex>
#include <sstream>
#include <string_view>
#include <vector>

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
constexpr std::string_view ClientIdHeaderName = "X-Axiom-Client-Id";

struct RemoteViewportWebSocketUserData {
  uintptr_t ConnectionId{0};
};

using UwsHttpRequest = uWS::HttpRequest;
using UwsHttpResponse = uWS::HttpResponse<false>;
using UwsWebSocket =
    uWS::WebSocket<false, true, RemoteViewportWebSocketUserData>;
std::function<bool(uintptr_t, std::string_view)> g_HttpResponseSender;

using SocketHandle = HAL::SocketHandle;

SocketHandle ToSocket(uintptr_t Value) { return Value; }

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

std::string Trim(std::string_view Value);

struct ParsedHttpResponse {
  std::string Status;
  std::vector<std::pair<std::string, std::string>> Headers;
  std::string Body;
};

std::optional<ParsedHttpResponse> ParseHttpResponseText(std::string_view Response) {
  const size_t HeaderEnd = Response.find("\r\n\r\n");
  if (HeaderEnd == std::string_view::npos) {
    return std::nullopt;
  }

  const std::string_view HeaderBlock = Response.substr(0, HeaderEnd);
  const size_t StatusLineEnd = HeaderBlock.find("\r\n");
  if (StatusLineEnd == std::string_view::npos) {
    return std::nullopt;
  }

  const std::string_view StatusLine = HeaderBlock.substr(0, StatusLineEnd);
  const size_t FirstSpace = StatusLine.find(' ');
  if (FirstSpace == std::string_view::npos || FirstSpace + 1 >= StatusLine.size()) {
    return std::nullopt;
  }

  ParsedHttpResponse Parsed{};
  Parsed.Status = std::string(StatusLine.substr(FirstSpace + 1));
  Parsed.Body = std::string(Response.substr(HeaderEnd + 4));

  size_t LineStart = StatusLineEnd + 2;
  while (LineStart < HeaderBlock.size()) {
    const size_t LineEnd = HeaderBlock.find("\r\n", LineStart);
    const std::string_view Line = HeaderBlock.substr(
        LineStart, (LineEnd == std::string_view::npos ? HeaderBlock.size() : LineEnd) -
                       LineStart);
    const size_t Colon = Line.find(':');
    if (Colon != std::string_view::npos) {
      Parsed.Headers.emplace_back(std::string(Trim(Line.substr(0, Colon))),
                                  std::string(Trim(Line.substr(Colon + 1))));
    }

    if (LineEnd == std::string_view::npos) {
      break;
    }
    LineStart = LineEnd + 2;
  }

  return Parsed;
}

std::string BuildHeaderBlock(std::string_view Method, UwsHttpRequest &Request) {
  std::ostringstream Stream;
  Stream << Method << ' ' << Request.getFullUrl() << " HTTP/1.1\r\n";
  for (const auto &[Key, Value] : Request) {
    Stream << Key << ": " << Value << "\r\n";
  }
  Stream << "\r\n";
  return Stream.str();
}

bool SendAll(SocketHandle Socket, const void *Data, size_t Size) {
  if (g_HttpResponseSender) {
    return g_HttpResponseSender(static_cast<uintptr_t>(Socket),
                                std::string_view(static_cast<const char *>(Data),
                                                 Size));
  }

  return HAL::SendSocketBytes(Socket, Data, Size);
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

std::string JsonResponse(std::string_view Status, std::string_view Payload) {
  return BuildHttpResponse(Status, "application/json; charset=utf-8", Payload);
}

using JsonWriter = rapidjson::Writer<rapidjson::StringBuffer>;

void WriteJsonString(JsonWriter &Writer, std::string_view Value) {
  Writer.String(Value.data(), static_cast<rapidjson::SizeType>(Value.size()));
}

template <typename Fn> std::string BuildJson(Fn &&FnWriter) {
  rapidjson::StringBuffer Buffer;
  JsonWriter Writer(Buffer);
  FnWriter(Writer);
  return std::string(Buffer.GetString(), Buffer.GetSize());
}

std::string SerializeTypeOnlyJson(std::string_view Type) {
  return BuildJson([&](JsonWriter &Writer) {
    Writer.StartObject();
    Writer.Key("type");
    WriteJsonString(Writer, Type);
    Writer.EndObject();
  });
}

std::optional<std::string> ExtractJsonStringField(std::string_view Body,
                                                  std::string_view FieldName) {
  std::string MutableBody(Body);
  rapidjson::Document Document;
  Document.ParseInsitu<rapidjson::kParseStopWhenDoneFlag>(MutableBody.data());
  if (Document.HasParseError() || !Document.IsObject()) {
    return std::nullopt;
  }

  const auto It = Document.FindMember(
      std::string(FieldName).c_str());
  if (It == Document.MemberEnd() || !It->value.IsString()) {
    return std::nullopt;
  }
  return std::string(It->value.GetString(), It->value.GetStringLength());
}

void WriteProjectJson(JsonWriter &Writer,
                      const Project::ProjectDescriptor &Project) {
  Writer.StartObject();
  Writer.Key("projectId");
  WriteJsonString(Writer, Project.Manifest.ProjectId);
  Writer.Key("name");
  WriteJsonString(Writer, Project.Manifest.Name);
  Writer.Key("slug");
  WriteJsonString(Writer, Project.Manifest.Slug);
  Writer.Key("rootPath");
  WriteJsonString(Writer, Project.Root.RootPath.string());
  Writer.Key("contentDir");
  WriteJsonString(Writer, Project.Root.ContentDir.string());
  Writer.Key("scriptsDir");
  WriteJsonString(Writer, Project.ScriptWorkspace.ScriptsDir.string());
  Writer.Key("scriptProjectPath");
  WriteJsonString(Writer, Project.ScriptWorkspace.ScriptProjectPath.string());
  Writer.Key("scriptSolutionPath");
  WriteJsonString(Writer, Project.ScriptWorkspace.ScriptSolutionPath.string());
  Writer.Key("scriptAssemblyName");
  WriteJsonString(Writer, Project.ScriptWorkspace.AssemblyName);
  Writer.Key("scriptRootNamespace");
  WriteJsonString(Writer, Project.ScriptWorkspace.RootNamespace);
  Writer.Key("starterScriptPath");
  WriteJsonString(Writer, Project.ScriptWorkspace.StarterScriptPath.string());
  Writer.Key("starterScriptClassName");
  WriteJsonString(Writer, Project.ScriptWorkspace.StarterScriptClassName);
  Writer.Key("starterScriptQualifiedClassName");
  WriteJsonString(Writer,
                  Project.ScriptWorkspace.StarterScriptQualifiedClassName);
  Writer.Key("cookedDir");
  WriteJsonString(Writer, Project.Output.CookedDir.string());
  Writer.Key("cookManifestPath");
  WriteJsonString(Writer, Project.Output.CookManifestPath.string());
  Writer.Key("buildDir");
  WriteJsonString(Writer, Project.Output.BuildDir.string());
  Writer.Key("packageDir");
  WriteJsonString(Writer, Project.Output.PackageDir.string());
  Writer.Key("packagedContentDir");
  WriteJsonString(Writer, Project.Output.PackagedContentDir.string());
  Writer.Key("packagedCookedDir");
  WriteJsonString(Writer, Project.Output.PackagedCookedDir.string());
  Writer.Key("packagedSceneAssetPath");
  WriteJsonString(Writer, Project.Output.PackagedSceneAssetPath.string());
  Writer.Key("stagedRuntimeBinaryPath");
  WriteJsonString(Writer, Project.Output.StagedRuntimeBinaryPath.string());
  Writer.Key("packageManifestPath");
  WriteJsonString(Writer, Project.Output.PackageManifestPath.string());
  Writer.Key("engineContentDir");
  WriteJsonString(
      Writer, (std::filesystem::path(AXIOM_CONTENT_DIR) / "Engine").string());
  Writer.Key("sceneFilePath");
  WriteJsonString(Writer, Project.Root.SceneFilePath.string());
  Writer.EndObject();
}

std::string SerializeProjectJson(const Project::ProjectDescriptor &Project) {
  return BuildJson([&](JsonWriter &Writer) { WriteProjectJson(Writer, Project); });
}

std::string SerializeProjectList(
    const std::vector<Project::ProjectDescriptor> &Projects,
    const std::optional<Project::ProjectDescriptor> &ActiveProject) {
  return BuildJson([&](JsonWriter &Writer) {
    Writer.StartObject();
    Writer.Key("type");
    Writer.String("projects");
    Writer.Key("activeProjectSlug");
    if (ActiveProject.has_value()) {
      WriteJsonString(Writer, ActiveProject->Manifest.Slug);
    } else {
      Writer.Null();
    }
    Writer.Key("projects");
    Writer.StartArray();
    for (const auto &Project : Projects) {
      WriteProjectJson(Writer, Project);
    }
    Writer.EndArray();
    Writer.EndObject();
  });
}

std::string SerializeCurrentProject(
    const std::optional<Project::ProjectDescriptor> &ActiveProject) {
  return BuildJson([&](JsonWriter &Writer) {
    Writer.StartObject();
    Writer.Key("type");
    Writer.String("current_project");
    Writer.Key("project");
    if (ActiveProject.has_value()) {
      WriteProjectJson(Writer, *ActiveProject);
    } else {
      Writer.Null();
    }
    Writer.EndObject();
  });
}

std::string SerializeProjectCookResult(
    const Project::ProjectDescriptor &Project,
    const Project::ProjectCookResult &Result) {
  return BuildJson([&](JsonWriter &Writer) {
    Writer.StartObject();
    Writer.Key("type");
    Writer.String("project_cooked");
    Writer.Key("project");
    WriteProjectJson(Writer, Project);
    Writer.Key("cookedSourceAssetCount");
    Writer.Uint64(Result.CookedSourceAssetCount);
    Writer.Key("manifestEntryCount");
    Writer.Uint64(Result.ManifestEntryCount);
    Writer.Key("cookManifestPath");
    WriteJsonString(Writer, Result.Output.CookManifestPath.string());
    Writer.EndObject();
  });
}

std::string SerializeProjectPackageResult(
    const Project::ProjectDescriptor &Project,
    const Project::ProjectPackageResult &Result) {
  return BuildJson([&](JsonWriter &Writer) {
    Writer.StartObject();
    Writer.Key("type");
    Writer.String("project_packaged");
    Writer.Key("project");
    WriteProjectJson(Writer, Project);
    Writer.Key("cookedSourceAssetCount");
    Writer.Uint64(Result.Cook.CookedSourceAssetCount);
    Writer.Key("manifestEntryCount");
    Writer.Uint64(Result.Cook.ManifestEntryCount);
    Writer.Key("packagedFileCount");
    Writer.Uint64(Result.PackagedFileCount);
    Writer.Key("includedSceneAsset");
    Writer.Bool(Result.IncludedSceneAsset);
    Writer.Key("includedEngineContent");
    Writer.Bool(Result.IncludedEngineContent);
    Writer.Key("includedRuntimeBinary");
    Writer.Bool(Result.IncludedRuntimeBinary);
    Writer.Key("sceneAssetPath");
    WriteJsonString(Writer, Result.SceneAssetPath.string());
    Writer.Key("runtimeBinaryPath");
    WriteJsonString(Writer, Result.RuntimeBinaryPath.string());
    Writer.Key("packagedContentPath");
    WriteJsonString(Writer, Result.Cook.Output.PackagedContentDir.string());
    Writer.Key("packageDir");
    WriteJsonString(Writer, Result.Cook.Output.PackageDir.string());
    Writer.Key("packageManifestPath");
    WriteJsonString(Writer, Result.Cook.Output.PackageManifestPath.string());
    Writer.EndObject();
  });
}

bool IsValidScriptRelativePath(std::filesystem::path RelativePath) {
  RelativePath = RelativePath.lexically_normal();
  if (RelativePath.empty() || RelativePath.is_absolute()) {
    return false;
  }
  if (RelativePath.filename().empty() || RelativePath.extension() != ".cs") {
    return false;
  }

  for (const auto &Part : RelativePath) {
    const auto Token = Part.string();
    if (Token.empty() || Token == "." || Token == "..") {
      return false;
    }
  }
  return true;
}

std::string SerializeScriptListJson(const std::vector<std::string> &Files) {
  return BuildJson([&](JsonWriter &Writer) {
    Writer.StartObject();
    Writer.Key("type");
    Writer.String("scripts_list");
    Writer.Key("files");
    Writer.StartArray();
    for (const auto &File : Files) {
      WriteJsonString(Writer, File);
    }
    Writer.EndArray();
    Writer.EndObject();
  });
}

std::string SerializeScriptFileJson(std::string_view RelativePath,
                                    std::string_view Content) {
  return BuildJson([&](JsonWriter &Writer) {
    Writer.StartObject();
    Writer.Key("type");
    Writer.String("script_file");
    Writer.Key("path");
    WriteJsonString(Writer, RelativePath);
    Writer.Key("content");
    WriteJsonString(Writer, Content);
    Writer.EndObject();
  });
}

std::string SerializeScriptMutationJson(std::string_view MutationType,
                                        std::string_view RelativePath) {
  return BuildJson([&](JsonWriter &Writer) {
    Writer.StartObject();
    Writer.Key("type");
    WriteJsonString(Writer, MutationType);
    Writer.Key("path");
    WriteJsonString(Writer, RelativePath);
    Writer.EndObject();
  });
}

std::string SerializeScriptClassesJson(
    const std::vector<std::pair<std::string, std::string>> &Classes) {
  return BuildJson([&](JsonWriter &Writer) {
    Writer.StartObject();
    Writer.Key("type");
    Writer.String("script_classes");
    Writer.Key("classes");
    Writer.StartArray();
    for (const auto &Entry : Classes) {
      Writer.StartObject();
      Writer.Key("className");
      WriteJsonString(Writer, Entry.first);
      Writer.Key("path");
      WriteJsonString(Writer, Entry.second);
      Writer.EndObject();
    }
    Writer.EndArray();
    Writer.EndObject();
  });
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

class RemoteViewportServer::ClientSessionRegistry {
public:
  explicit ClientSessionRegistry(RemoteViewportServer &Server)
      : m_Server(Server) {}

  size_t GetRemoteClientCount() const {
    std::scoped_lock Lock(m_RemoteClientMutex);
    return m_RemoteClientsById.size();
  }

  size_t GetActiveWebRtcSessionCount() const {
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
  CollectPresenceEntries() const {
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

  std::optional<SessionUserId> ResolveClientUser(std::string_view ClientId) const {
    std::scoped_lock Lock(m_RemoteClientMutex);
    const auto It = m_RemoteClientsById.find(std::string(ClientId));
    if (It == m_RemoteClientsById.end()) {
      return std::nullopt;
    }
    return It->second->User;
  }

  std::shared_ptr<RemoteClientSession> Find(std::string_view ClientId) {
    std::scoped_lock Lock(m_RemoteClientMutex);
    const auto It = m_RemoteClientsById.find(std::string(ClientId));
    return It != m_RemoteClientsById.end() ? It->second : nullptr;
  }

  std::shared_ptr<const RemoteClientSession> Find(std::string_view ClientId) const {
    std::scoped_lock Lock(m_RemoteClientMutex);
    const auto It = m_RemoteClientsById.find(std::string(ClientId));
    return It != m_RemoteClientsById.end() ? It->second : nullptr;
  }

  WebRtcSessionStatus GetWebRtcStatus(std::string_view ClientId) const {
    const auto Client = Find(ClientId);
    if (Client == nullptr || Client->WebRtcSession == nullptr) {
      return {};
    }
    return Client->WebRtcSession->GetStatus();
  }

  std::vector<IWebRtcSession *> CollectWebRtcSessions() const {
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

  ClientSessionResolution CreateOrResume(
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
            std::make_unique<RemoteClientSession::PacketOutput>(
                m_Server, Session->ClientId);
        if (Session->VideoEncoder != nullptr &&
            Session->VideoPacketOutput != nullptr) {
          Session->VideoEncoder->SetOutput(Session->VideoPacketOutput.get());
        }
        if (Session->WebRtcSession != nullptr) {
          const std::string ClientId = Session->ClientId;
          Session->WebRtcSession->SetCommandMessageHandler(
              [this, ClientId](std::string_view Payload) {
                m_Server.HandleClientWebRtcMessage(ClientId, Payload);
              });
        }
        m_RemoteClientsById.emplace(Session->ClientId, Session);
        ResolvedSession = std::move(Session);
      }
    }

    m_Server.m_Host.GetHeadlessLayer().GetSession().EnsureViewportState(
        ResolvedSession->User);
    m_Server.m_Host.GetHeadlessLayer().GetSession().SetPresenceState(
        ResolvedSession->User, EditorUserPresenceState::Connected);
    m_Server.m_Host.EnsureRemoteRenderView(ResolvedSession->ClientId,
                                           ResolvedSession->User);
    return {.Session = std::move(ResolvedSession),
            .ResumedExisting = ResumedExisting};
  }

  void Touch(const std::string &ClientId) {
    {
      std::scoped_lock Lock(m_RemoteClientMutex);
      const auto It = m_RemoteClientsById.find(ClientId);
      if (It != m_RemoteClientsById.end()) {
        It->second->LastActivity = std::chrono::steady_clock::now();
      }
    }
    m_Server.m_Host.FocusRemoteRenderView(ClientId);
  }

private:
  RemoteViewportServer &m_Server;
  mutable std::mutex m_RemoteClientMutex;
  std::unordered_map<std::string, std::shared_ptr<RemoteClientSession>>
      m_RemoteClientsById;
  uint64_t m_NextRemoteUserId{2};
};

class RemoteViewportServer::ProjectWorkspaceService {
public:
  explicit ProjectWorkspaceService(RemoteViewportServer &Server)
      : m_Server(Server), m_ProjectsRoot(Project::GetDefaultProjectsRoot()) {}

  std::vector<Project::ProjectDescriptor> ListProjects() const {
    return Project::DiscoverProjects(m_ProjectsRoot);
  }

  std::optional<Project::ProjectDescriptor> GetActiveProject() const {
    std::scoped_lock Lock(m_ProjectMutex);
    return m_ActiveProject;
  }

  std::optional<Project::ProjectDescriptor>
  SetActiveProjectBySlug(std::string_view ProjectSlug) {
    const auto Opened = Project::OpenProjectBySlug(m_ProjectsRoot, ProjectSlug);
    if (!Opened.has_value()) {
      return std::nullopt;
    }

    std::scoped_lock Lock(m_ProjectMutex);
    m_ActiveProject = *Opened;
    return Opened;
  }

  void SetActiveProject(const Project::ProjectDescriptor &Project) {
    std::scoped_lock Lock(m_ProjectMutex);
    m_ActiveProject = Project;
  }

  std::filesystem::path GetActiveContentDir() const {
    if (const auto ActiveProject = GetActiveProject(); ActiveProject.has_value()) {
      return ActiveProject->Root.ContentDir;
    }
    return std::filesystem::path(AXIOM_CONTENT_DIR);
  }

  std::filesystem::path GetActiveScriptsDir() const {
    if (const auto ActiveProject = GetActiveProject(); ActiveProject.has_value()) {
      return ActiveProject->ScriptWorkspace.ScriptsDir;
    }
    return std::filesystem::path(AXIOM_PROJECTS_DIR) / "__default__" / "Scripts";
  }

  std::filesystem::path GetEngineContentDir() const {
    return std::filesystem::path(AXIOM_CONTENT_DIR) / "Engine";
  }

  bool LoadActiveProjectIntoSession(std::string *FailureReason) {
    if (m_Server.m_Host.LoadStartupSceneIntoSession(GetActiveContentDir())) {
      return true;
    }

    if (FailureReason != nullptr) {
      *FailureReason =
          "Failed to load the active project's startup scene into the session.";
    }
    return false;
  }

  std::vector<std::string> ListScriptFiles() const {
    std::vector<std::string> Results;
    const auto ScriptsDir = GetActiveScriptsDir();
    if (!std::filesystem::exists(ScriptsDir)) {
      return Results;
    }

    for (const auto &Entry :
         std::filesystem::recursive_directory_iterator(ScriptsDir)) {
      if (!Entry.is_regular_file() || Entry.path().extension() != ".cs") {
        continue;
      }

      std::error_code Error;
      const auto Relative = std::filesystem::relative(Entry.path(), ScriptsDir, Error);
      if (!Error) {
        Results.push_back(Relative.generic_string());
      }
    }

    std::sort(Results.begin(), Results.end());
    return Results;
  }

  std::vector<std::pair<std::string, std::string>> ListScriptClasses() const {
    std::vector<std::pair<std::string, std::string>> Results;
    const auto ScriptFiles = ListScriptFiles();
    const auto ActiveProject = GetActiveProject();
    const std::string DefaultNamespace =
        ActiveProject.has_value() ? ActiveProject->ScriptWorkspace.RootNamespace
                                  : "Project.Scripts";
    const std::regex NamespacePattern(
        R"(namespace\s+([A-Za-z_][A-Za-z0-9_\.]*)\s*[;{])");
    const std::regex ClassPattern(
        R"(public\s+class\s+([A-Za-z_][A-Za-z0-9_]*)\s*:\s*Script\b)");

    for (const auto &RelativePath : ScriptFiles) {
      const auto AbsolutePath = ResolveActiveScriptPath(RelativePath);
      if (!AbsolutePath.has_value()) {
        continue;
      }

      std::ifstream File(*AbsolutePath);
      if (!File.is_open()) {
        continue;
      }

      const std::string Content((std::istreambuf_iterator<char>(File)),
                                std::istreambuf_iterator<char>());
      std::string Namespace = DefaultNamespace;
      if (std::smatch NamespaceMatch;
          std::regex_search(Content, NamespaceMatch, NamespacePattern) &&
          NamespaceMatch.size() > 1) {
        Namespace = NamespaceMatch[1].str();
      }

      auto ClassBegin =
          std::sregex_iterator(Content.begin(), Content.end(), ClassPattern);
      const auto ClassEnd = std::sregex_iterator();
      for (auto It = ClassBegin; It != ClassEnd; ++It) {
        Results.emplace_back(Namespace + "." + (*It)[1].str(), RelativePath);
      }
    }

    std::sort(Results.begin(), Results.end(),
              [](const auto &Left, const auto &Right) {
                return Left.first < Right.first;
              });
    return Results;
  }

  std::optional<std::filesystem::path>
  ResolveActiveScriptPath(std::string_view RelativePath,
                          bool AllowMissingLeaf = false) const {
    const std::filesystem::path Relative =
        std::filesystem::path(RelativePath).lexically_normal();
    if (!IsValidScriptRelativePath(Relative)) {
      return std::nullopt;
    }

    const auto ScriptsDir = GetActiveScriptsDir();
    const auto Candidate = (ScriptsDir / Relative).lexically_normal();
    const auto ValidationPath = AllowMissingLeaf ? Candidate.parent_path() : Candidate;
    if (!Project::IsPathWithinRoot(ScriptsDir, ValidationPath)) {
      return std::nullopt;
    }
    return Candidate;
  }

private:
  RemoteViewportServer &m_Server;
  const std::filesystem::path m_ProjectsRoot;
  mutable std::mutex m_ProjectMutex;
  std::optional<Project::ProjectDescriptor> m_ActiveProject;
};

class RemoteViewportServer::AssetLibraryService {
public:
  AssetLibraryService(RemoteViewportServer &Server,
                      ProjectWorkspaceService &Workspace)
      : m_Server(Server), m_Workspace(Workspace) {}

  std::vector<Assets::AssetDescriptor> CollectVisibleAssets() const {
    std::vector<Assets::AssetDescriptor> Assets;

    const Assets::LocalAssetSource ProjectSource{m_Workspace.GetActiveContentDir()};
    Assets = ProjectSource.List();

    const Assets::LocalAssetSource EngineSource{m_Workspace.GetEngineContentDir()};
    for (auto EngineAsset : EngineSource.List()) {
      EngineAsset.RelativePath =
          (std::filesystem::path("Engine") / EngineAsset.RelativePath)
              .generic_string();
      EngineAsset.Name =
          std::filesystem::path(EngineAsset.RelativePath).stem().string();
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
  ResolveVisibleAssetPath(std::string_view RelativePath) const {
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
      return m_Workspace.GetEngineContentDir() / EngineRelative;
    }

    return m_Workspace.GetActiveContentDir() / Relative;
  }

private:
  RemoteViewportServer &m_Server;
  ProjectWorkspaceService &m_Workspace;
};

class RemoteViewportServer::BrowserCommandRouter {
public:
  BrowserCommandRouter(RemoteViewportServer &Server, ClientSessionRegistry &Registry,
                       ProjectWorkspaceService &Workspace,
                       AssetLibraryService &Assets)
      : m_Server(Server),
        m_Registry(Registry),
        m_Workspace(Workspace),
        m_Assets(Assets) {}

  bool HandleWebSocketMessage(uintptr_t ClientSocketValue, std::string_view Payload);
  bool HandleClientWebRtcMessage(std::string_view ClientId,
                                 std::string_view Payload);

private:
  RemoteViewportServer &m_Server;
  ClientSessionRegistry &m_Registry;
  ProjectWorkspaceService &m_Workspace;
  AssetLibraryService &m_Assets;
};

RemoteViewportServer::RemoteViewportServer(
    HeadlessSessionHost &Host, const RemoteViewportServerOptions &Options)
    : m_Host(Host), m_Options(Options) {
  m_ClientRegistry = std::make_unique<ClientSessionRegistry>(*this);
  m_ProjectWorkspace = std::make_unique<ProjectWorkspaceService>(*this);
  m_AssetLibrary =
      std::make_unique<AssetLibraryService>(*this, *m_ProjectWorkspace);
  m_CommandRouter = std::make_unique<BrowserCommandRouter>(
      *this, *m_ClientRegistry, *m_ProjectWorkspace, *m_AssetLibrary);
  m_Host.SetTransportVideoEncoder(nullptr);
}

RemoteViewportServer::~RemoteViewportServer() { Stop(); }

RemoteViewportServerMetrics RemoteViewportServer::GetMetrics() const {
  RemoteViewportServerMetrics Metrics{};
  Metrics.TransportConnected = m_TransportConnected.load();
  Metrics.ListenPort = m_Options.Port;
  Metrics.TotalHttpRequests = m_TotalHttpRequests.load();
  Metrics.TotalWebSocketMessages = m_TotalWebSocketMessages.load();
  {
    std::scoped_lock Lock(m_WebSocketMutex);
    Metrics.ActiveWebSocketClients = m_WebSocketClients.size();
  }
  Metrics.ActiveRemoteClients = m_ClientRegistry->GetRemoteClientCount();
  Metrics.ActiveWebRtcSessions = m_ClientRegistry->GetActiveWebRtcSessionCount();
  return Metrics;
}

bool RemoteViewportServer::Start(std::string &Error) {
  m_StopRequested.store(false);
  g_HttpResponseSender = [this](uintptr_t ClientSocketValue,
                                std::string_view Response) {
    return SendHttpResponse(ClientSocketValue, Response);
  };
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
      const uintptr_t ConnectionId = AllocateConnectionId();
      RegisterPendingHttpResponse(ConnectionId, Response);
      Response->onAborted([this, ConnectionId]() {
        MarkPendingHttpResponseAborted(ConnectionId);
      });

      const std::string HeaderBlock = BuildHeaderBlock("GET", *Request);
      m_TotalHttpRequests.fetch_add(1);
      HandleGetRequest(ConnectionId, std::string(Request->getFullUrl()),
                       HeaderBlock);
    };

    auto RegisterOptionsHandler = [this](UwsHttpResponse *Response,
                                         UwsHttpRequest *Request) {
      (void)Request;
      const uintptr_t ConnectionId = AllocateConnectionId();
      RegisterPendingHttpResponse(ConnectionId, Response);
      Response->onAborted([this, ConnectionId]() {
        MarkPendingHttpResponseAborted(ConnectionId);
      });

      m_TotalHttpRequests.fetch_add(1);
      const std::string ResponseText = BuildHttpResponse(
          "204 No Content", "text/plain; charset=utf-8", "",
          "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
          "Access-Control-Allow-Headers: Content-Type, X-Axiom-Client-Id\r\n");
      SendHttpResponse(ConnectionId, ResponseText);
    };

    auto RegisterPostHandler = [this](UwsHttpResponse *Response,
                                      UwsHttpRequest *Request) {
      const uintptr_t ConnectionId = AllocateConnectionId();
      RegisterPendingHttpResponse(ConnectionId, Response);
      Response->onAborted([this, ConnectionId]() {
        MarkPendingHttpResponseAborted(ConnectionId);
      });

      auto HeaderBlock =
          std::make_shared<std::string>(BuildHeaderBlock("POST", *Request));
      auto Path = std::make_shared<std::string>(Request->getFullUrl());
      auto Body = std::make_shared<std::string>();
      m_TotalHttpRequests.fetch_add(1);

      const std::string_view ContentLength = Request->getHeader("content-length");
      if (ContentLength.empty() || ContentLength == "0") {
        HandlePostRequest(ConnectionId, *Path, *HeaderBlock, "");
        return;
      }

      Response->onData([this, ConnectionId, HeaderBlock, Path, Body](
                           std::string_view Chunk, bool IsLast) {
        Body->append(Chunk.data(), Chunk.size());
        if (IsLast) {
          HandlePostRequest(ConnectionId, *Path, *HeaderBlock, *Body);
        }
      });
    };

    uWS::App::WebSocketBehavior<RemoteViewportWebSocketUserData> Behavior{};
    Behavior.compression = uWS::DISABLED;
    Behavior.maxPayloadLength = 256 * 1024;
    Behavior.upgrade =
        [this](UwsHttpResponse *Response, UwsHttpRequest *Request,
               us_socket_context_t *Context) {
          const uintptr_t ConnectionId = AllocateConnectionId();
          Response->template upgrade<RemoteViewportWebSocketUserData>(
              {.ConnectionId = ConnectionId},
              Request->getHeader("sec-websocket-key"),
              Request->getHeader("sec-websocket-protocol"),
              Request->getHeader("sec-websocket-extensions"), Context);
        };
    Behavior.open = [this](UwsWebSocket *Socket) {
      const uintptr_t ConnectionId = Socket->getUserData()->ConnectionId;
      {
        std::scoped_lock Lock(m_WebSocketMutex);
        m_WebSocketClients.push_back(
            {.ConnectionId = ConnectionId, .Socket = Socket, .IsOpen = true});
      }
      std::cout << SerializeConnected() << std::endl;
      SendTextMessage(ConnectionId,
                      SerializeReady(m_Options.Width, m_Options.Height));
      SendTextMessage(ConnectionId, SerializeConnected());
    };
    Behavior.message = [this](UwsWebSocket *Socket, std::string_view Message,
                              uWS::OpCode OpCode) {
      if (OpCode != uWS::OpCode::TEXT) {
        return;
      }
      const uintptr_t ConnectionId = Socket->getUserData()->ConnectionId;
      if (!HandleWebSocketMessage(ConnectionId, Message)) {
        SendTextMessage(ConnectionId,
                        SerializeError("Invalid WebSocket command payload."));
      }
    };
    Behavior.close = [this](UwsWebSocket *Socket, int Code,
                            std::string_view Message) {
      (void)Code;
      (void)Message;
      RemoveWebSocketClient(Socket->getUserData()->ConnectionId);
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

  m_PresenceThread = std::thread([this]() { PresenceLoop(); });
  return true;
}

void RemoteViewportServer::Stop() {
  const bool WasStopping = m_StopRequested.exchange(true);
  if (WasStopping) {
    return;
  }

  CloseAllClients();
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
  for (IWebRtcSession *Session : CollectClientWebRtcSessions()) {
    Session->ResetPeer("server_stopped");
  }
  g_HttpResponseSender = nullptr;
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

    for (const auto &[User, LastActivity] : m_ClientRegistry->CollectPresenceEntries()) {
      const auto Elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                               Now - LastActivity)
                               .count();
        const EditorUserPresence *Presence =
            m_Host.GetHeadlessLayer().GetSession().FindPresence(User);
        if (Presence == nullptr) {
          continue;
        }
        if (Elapsed >= DisconnectThresholdSeconds &&
            Presence->State == EditorUserPresenceState::Away) {
          Transitions.emplace_back(User, EditorUserPresenceState::Disconnected);
        } else if (Elapsed >= AwayThresholdSeconds &&
                   Presence->State == EditorUserPresenceState::Connected) {
          Transitions.emplace_back(User, EditorUserPresenceState::Away);
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

void RemoteViewportServer::BroadcastTextMessage(std::string Message) {
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
    RemoveWebSocketClient(FailedClient);
  }
}

void RemoteViewportServer::CloseAllClients() {
  {
    std::scoped_lock Lock(m_WebSocketMutex);
    for (auto &Client : m_WebSocketClients) {
      Client.IsOpen = false;
    }
    m_WebSocketClients.clear();
  }
  {
    std::scoped_lock Lock(m_HttpResponseMutex);
    m_PendingHttpResponses.clear();
  }
}

void RemoteViewportServer::RemoveWebSocketClient(uintptr_t ClientSocketValue) {
  bool Removed = false;
  {
    std::scoped_lock Lock(m_WebSocketMutex);
    auto It = std::find_if(m_WebSocketClients.begin(), m_WebSocketClients.end(),
                           [ClientSocketValue](const WebSocketClient &Client) {
                             return Client.ConnectionId == ClientSocketValue;
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

bool RemoteViewportServer::SendTextMessage(uintptr_t ClientSocketValue,
                                           std::string_view Message) {
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

bool RemoteViewportServer::SendBinaryMessage(uintptr_t ClientSocketValue,
                                             const void *Data, size_t Size) {
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

bool RemoteViewportServer::SendHttpResponse(uintptr_t ClientSocketValue,
                                            std::string_view Response) {
  PendingHttpResponse Pending{};
  {
    std::scoped_lock Lock(m_HttpResponseMutex);
    const auto It = m_PendingHttpResponses.find(ClientSocketValue);
    if (It == m_PendingHttpResponses.end() || It->second.Aborted) {
      return false;
    }
    Pending = It->second;
    m_PendingHttpResponses.erase(It);
  }

  auto Parsed = ParseHttpResponseText(Response);
  if (!Parsed.has_value()) {
    return false;
  }

  auto *HttpResponse = static_cast<UwsHttpResponse *>(Pending.Response);
  HttpResponse->writeStatus(Parsed->Status);
  for (const auto &[HeaderName, HeaderValue] : Parsed->Headers) {
    if (EqualsCaseInsensitive(HeaderName, "Content-Length") ||
        EqualsCaseInsensitive(HeaderName, "Connection")) {
      continue;
    }
    HttpResponse->writeHeader(HeaderName, HeaderValue);
  }
  HttpResponse->end(Parsed->Body);
  return true;
}

uintptr_t RemoteViewportServer::AllocateConnectionId() {
  return m_NextClientConnectionId.fetch_add(1);
}

void RemoteViewportServer::RegisterPendingHttpResponse(uintptr_t ClientSocketValue,
                                                       void *Response) {
  std::scoped_lock Lock(m_HttpResponseMutex);
  m_PendingHttpResponses[ClientSocketValue] = PendingHttpResponse{
      .Response = Response,
      .Aborted = false,
  };
}

void RemoteViewportServer::MarkPendingHttpResponseAborted(
    uintptr_t ClientSocketValue) {
  std::scoped_lock Lock(m_HttpResponseMutex);
  auto It = m_PendingHttpResponses.find(ClientSocketValue);
  if (It != m_PendingHttpResponses.end()) {
    It->second.Aborted = true;
    m_PendingHttpResponses.erase(It);
  }
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
  if (Route == "/projects/cook") {
    return HandleCookProjectRequest(ClientSocketValue);
  }
  if (Route == "/projects/package") {
    return HandlePackageProjectRequest(ClientSocketValue);
  }
  if (Route == "/scripts/create") {
    return HandleCreateScriptFileRequest(ClientSocketValue, Body);
  }
  if (Route == "/scripts/save") {
    return HandleSaveScriptFileRequest(ClientSocketValue, Body);
  }
  if (Route == "/scripts/rename") {
    return HandleRenameScriptFileRequest(ClientSocketValue, Body);
  }
  if (Route == "/scripts/delete") {
    return HandleDeleteScriptFileRequest(ClientSocketValue, Body);
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
  case HeadlessCommandType::SetShowColliders:
    m_Host.SetRemoteShowColliders(*User, Command->ShowColliders);
    break;
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
    m_Host.SubmitRemoteCommand(*User, Command->EditorPayload);
    break;
  case HeadlessCommandType::DropMesh:
    HandleMeshDropCommand(*User, *Command);
    break;
  case HeadlessCommandType::DropTexture: {
    HandleTextureDropCommand(*User, *Command);
    break;
  }
  case HeadlessCommandType::PlaceActor:
    HandlePlaceActorCommand(*User, *Command);
    break;
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
      JsonResponse("202 Accepted", SerializeTypeOnlyJson("accepted"));
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
  const auto Created = Project::CreateProjectScaffold(
      Project::GetDefaultProjectsRoot(), *ProjectName, &FailureReason);
  if (!Created.has_value()) {
    const std::string Response = JsonResponse(
        "400 Bad Request", SerializeError(FailureReason));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  m_ProjectWorkspace->SetActiveProject(*Created);

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

bool RemoteViewportServer::HandleCookProjectRequest(uintptr_t ClientSocketValue) {
  const SocketHandle ClientSocket = ToSocket(ClientSocketValue);
  const auto ActiveProject = GetActiveProject();
  if (!ActiveProject.has_value()) {
    const std::string Response =
        JsonResponse("400 Bad Request",
                     SerializeError("No active project is selected."));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  std::string FailureReason;
  const auto Result =
      Project::CookProjectContent(*ActiveProject, &FailureReason);
  if (!Result.has_value()) {
    const std::string Response =
        JsonResponse("500 Internal Server Error", SerializeError(FailureReason));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  const std::string Response =
      JsonResponse("200 OK", SerializeProjectCookResult(*ActiveProject, *Result));
  SendAll(ClientSocket, Response.data(), Response.size());
  return false;
}

bool RemoteViewportServer::HandlePackageProjectRequest(
    uintptr_t ClientSocketValue) {
  const SocketHandle ClientSocket = ToSocket(ClientSocketValue);
  const auto ActiveProject = GetActiveProject();
  if (!ActiveProject.has_value()) {
    const std::string Response =
        JsonResponse("400 Bad Request",
                     SerializeError("No active project is selected."));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  std::string FailureReason;
  const auto Result =
      Project::PackageProjectContent(*ActiveProject, &FailureReason);
  if (!Result.has_value()) {
    const std::string Response =
        JsonResponse("500 Internal Server Error", SerializeError(FailureReason));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  const std::string Response = JsonResponse(
      "200 OK", SerializeProjectPackageResult(*ActiveProject, *Result));
  SendAll(ClientSocket, Response.data(), Response.size());
  return false;
}

bool RemoteViewportServer::HandleListScriptsRequest(uintptr_t ClientSocketValue) {
  const SocketHandle ClientSocket = ToSocket(ClientSocketValue);
  const std::string Response =
      JsonResponse("200 OK", SerializeScriptListJson(ListScriptFiles()));
  SendAll(ClientSocket, Response.data(), Response.size());
  return false;
}

bool RemoteViewportServer::HandleListScriptClassesRequest(
    uintptr_t ClientSocketValue) {
  const SocketHandle ClientSocket = ToSocket(ClientSocketValue);
  const std::string Response =
      JsonResponse("200 OK", SerializeScriptClassesJson(ListScriptClasses()));
  SendAll(ClientSocket, Response.data(), Response.size());
  return false;
}

bool RemoteViewportServer::HandleReadScriptFileRequest(uintptr_t ClientSocketValue,
                                                       std::string_view Path) {
  const SocketHandle ClientSocket = ToSocket(ClientSocketValue);
  const auto RelativePath = GetQueryParam(Path, "path");
  if (!RelativePath.has_value() || RelativePath->empty()) {
    const std::string Response = JsonResponse(
        "400 Bad Request", SerializeError("Missing required 'path' query parameter."));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  const auto FilePath = ResolveActiveScriptPath(*RelativePath);
  if (!FilePath.has_value() || !std::filesystem::exists(*FilePath)) {
    const std::string Response = JsonResponse(
        "404 Not Found", SerializeError("Script file was not found."));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  std::ifstream File(*FilePath);
  if (!File.is_open()) {
    const std::string Response = JsonResponse(
        "500 Internal Server Error", SerializeError("Failed to open script file."));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  const std::string Content((std::istreambuf_iterator<char>(File)),
                            std::istreambuf_iterator<char>());
  const std::string Response =
      JsonResponse("200 OK", SerializeScriptFileJson(*RelativePath, Content));
  SendAll(ClientSocket, Response.data(), Response.size());
  return false;
}

bool RemoteViewportServer::HandleCreateScriptFileRequest(
    uintptr_t ClientSocketValue, std::string_view Body) {
  const SocketHandle ClientSocket = ToSocket(ClientSocketValue);
  const auto RelativePath = ExtractJsonStringField(Body, "path");
  if (!RelativePath.has_value() || RelativePath->empty()) {
    const std::string Response =
        JsonResponse("400 Bad Request",
                     SerializeError("Missing required 'path' field."));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  const auto FilePath = ResolveActiveScriptPath(*RelativePath, true);
  if (!FilePath.has_value()) {
    const std::string Response = JsonResponse(
        "400 Bad Request",
        SerializeError("Script path must stay inside the active project's Scripts directory and end in .cs."));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }
  if (std::filesystem::exists(*FilePath)) {
    const std::string Response = JsonResponse(
        "409 Conflict", SerializeError("A script file with that path already exists."));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  std::error_code Error;
  std::filesystem::create_directories(FilePath->parent_path(), Error);
  if (Error) {
    const std::string Response = JsonResponse(
        "500 Internal Server Error",
        SerializeError("Failed to create the script directory."));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  std::ofstream File(*FilePath);
  if (!File.is_open()) {
    const std::string Response = JsonResponse(
        "500 Internal Server Error", SerializeError("Failed to create script file."));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  const auto ActiveProject = GetActiveProject();
  const std::string Namespace = ActiveProject.has_value()
                                    ? ActiveProject->ScriptWorkspace.RootNamespace
                                    : "Project.Scripts";
  const std::string ClassName = FilePath->stem().string();
  std::ostringstream Template;
  Template << "using WraithEngine;\n\n"
           << "namespace " << Namespace << ";\n\n"
           << "public class " << ClassName << " : Script\n"
           << "{\n"
           << "    public override void OnCreate()\n"
           << "    {\n"
           << "    }\n\n"
           << "    public override void OnTick(float dt)\n"
           << "    {\n"
           << "    }\n"
           << "}\n";
  File << Template.str();
  File.close();

  const std::string Response =
      JsonResponse("201 Created", SerializeScriptMutationJson("script_created", *RelativePath));
  SendAll(ClientSocket, Response.data(), Response.size());
  return false;
}

bool RemoteViewportServer::HandleSaveScriptFileRequest(uintptr_t ClientSocketValue,
                                                       std::string_view Body) {
  const SocketHandle ClientSocket = ToSocket(ClientSocketValue);
  const auto RelativePath = ExtractJsonStringField(Body, "path");
  const auto Content = ExtractJsonStringField(Body, "content");
  if (!RelativePath.has_value() || !Content.has_value()) {
    const std::string Response =
        JsonResponse("400 Bad Request",
                     SerializeError("Missing required 'path' or 'content' field."));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  const auto FilePath = ResolveActiveScriptPath(*RelativePath, true);
  if (!FilePath.has_value()) {
    const std::string Response = JsonResponse(
        "400 Bad Request",
        SerializeError("Script path must stay inside the active project's Scripts directory and end in .cs."));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  std::error_code Error;
  std::filesystem::create_directories(FilePath->parent_path(), Error);
  if (Error) {
    const std::string Response = JsonResponse(
        "500 Internal Server Error",
        SerializeError("Failed to create the script directory."));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  std::ofstream File(*FilePath);
  if (!File.is_open()) {
    const std::string Response = JsonResponse(
        "500 Internal Server Error", SerializeError("Failed to save script file."));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }
  File << *Content;
  File.close();

  const std::string Response =
      JsonResponse("200 OK", SerializeScriptMutationJson("script_saved", *RelativePath));
  SendAll(ClientSocket, Response.data(), Response.size());
  return false;
}

bool RemoteViewportServer::HandleRenameScriptFileRequest(
    uintptr_t ClientSocketValue, std::string_view Body) {
  const SocketHandle ClientSocket = ToSocket(ClientSocketValue);
  const auto RelativePath = ExtractJsonStringField(Body, "path");
  const auto NewRelativePath = ExtractJsonStringField(Body, "newPath");
  if (!RelativePath.has_value() || !NewRelativePath.has_value() ||
      RelativePath->empty() || NewRelativePath->empty()) {
    const std::string Response =
        JsonResponse("400 Bad Request",
                     SerializeError("Missing required 'path' or 'newPath' field."));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  const auto OldPath = ResolveActiveScriptPath(*RelativePath);
  const auto NewPath = ResolveActiveScriptPath(*NewRelativePath, true);
  if (!OldPath.has_value() || !NewPath.has_value() ||
      !std::filesystem::exists(*OldPath)) {
    const std::string Response = JsonResponse(
        "400 Bad Request",
        SerializeError("Script rename must stay inside the active project's Scripts directory and target an existing .cs file."));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }
  if (std::filesystem::exists(*NewPath)) {
    const std::string Response = JsonResponse(
        "409 Conflict", SerializeError("A script file with the destination path already exists."));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  std::error_code Error;
  std::filesystem::create_directories(NewPath->parent_path(), Error);
  if (Error) {
    const std::string Response = JsonResponse(
        "500 Internal Server Error",
        SerializeError("Failed to create the destination script directory."));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }
  std::filesystem::rename(*OldPath, *NewPath, Error);
  if (Error) {
    const std::string Response = JsonResponse(
        "500 Internal Server Error", SerializeError("Failed to rename script file."));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  const std::string Response =
      JsonResponse("200 OK", SerializeScriptMutationJson("script_renamed", *NewRelativePath));
  SendAll(ClientSocket, Response.data(), Response.size());
  return false;
}

bool RemoteViewportServer::HandleDeleteScriptFileRequest(
    uintptr_t ClientSocketValue, std::string_view Body) {
  const SocketHandle ClientSocket = ToSocket(ClientSocketValue);
  const auto RelativePath = ExtractJsonStringField(Body, "path");
  if (!RelativePath.has_value() || RelativePath->empty()) {
    const std::string Response =
        JsonResponse("400 Bad Request",
                     SerializeError("Missing required 'path' field."));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  const auto FilePath = ResolveActiveScriptPath(*RelativePath);
  if (!FilePath.has_value() || !std::filesystem::exists(*FilePath)) {
    const std::string Response = JsonResponse(
        "404 Not Found", SerializeError("Script file was not found."));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  std::error_code Error;
  const bool Removed = std::filesystem::remove(*FilePath, Error);
  if (Error || !Removed) {
    const std::string Response = JsonResponse(
        "500 Internal Server Error", SerializeError("Failed to delete script file."));
    SendAll(ClientSocket, Response.data(), Response.size());
    return false;
  }

  const std::string Response =
      JsonResponse("200 OK", SerializeScriptMutationJson("script_deleted", *RelativePath));
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
  const bool ShowColliders =
      [&]() -> bool {
        if (const HeadlessRenderViewState *View =
                m_Host.FindRemoteRenderView(Client.ClientId);
            View != nullptr) {
          return View->ShowColliders;
        }
        return true;
      }();
  const std::string Payload = SerializeSessionConnectResponse(
      Client.ClientId, m_Host.GetHeadlessLayer().GetSession().GetState(),
      Client.User, ShowColliders, m_TransportConnected.load(),
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
  if (Route == "/scripts") {
    return HandleListScriptsRequest(ClientSocketValue);
  }
  if (Route == "/scripts/classes") {
    return HandleListScriptClassesRequest(ClientSocketValue);
  }
  if (Route == "/scripts/file") {
    return HandleReadScriptFileRequest(ClientSocketValue, Path);
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
      if (auto Client = FindClientSession(*ClientId);
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
    const bool ShowColliders =
        [&]() -> bool {
          if (ClientId.has_value()) {
            if (const HeadlessRenderViewState *View =
                    m_Host.FindRemoteRenderView(*ClientId);
                View != nullptr) {
              return View->ShowColliders;
            }
          }
          if (const HeadlessRenderViewState *View = m_Host.FindRenderView(*User);
              View != nullptr) {
            return View->ShowColliders;
          }
          return true;
        }();
    const std::string Body = SerializeSessionSnapshot(
        m_Host.GetHeadlessLayer().GetSession().GetState(), *User,
        ShowColliders,
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
  auto Client = FindClientSession(*ClientId);
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
  auto Client = FindClientSession(*ClientId);
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
      JsonResponse("202 Accepted", SerializeTypeOnlyJson("accepted"));
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
  auto Client = FindClientSession(*ClientId);
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
    if (const auto Client = FindClientSession(*ClientId); Client != nullptr) {
      DisconnectedUser = Client->User;
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
        ".glb", ".gltf", ".fbx", ".obj", ".png", ".jpg", ".jpeg", ".hdr"};
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

  const std::string Payload = BuildJson([&](JsonWriter &Writer) {
    Writer.StartObject();
    Writer.Key("type");
    Writer.String("assets_uploaded");
    Writer.Key("files");
    Writer.StartArray();
    for (const auto &SavedPath : Saved) {
      WriteJsonString(Writer, SavedPath);
    }
    Writer.EndArray();
    Writer.EndObject();
  });
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
  return m_ClientRegistry->ResolveClientUser(*ClientId);
}

std::shared_ptr<RemoteViewportServer::RemoteClientSession>
RemoteViewportServer::FindClientSession(std::string_view ClientId) {
  return m_ClientRegistry->Find(ClientId);
}

std::shared_ptr<const RemoteViewportServer::RemoteClientSession>
RemoteViewportServer::FindClientSession(std::string_view ClientId) const {
  return m_ClientRegistry->Find(ClientId);
}

WebRtcSessionStatus
RemoteViewportServer::GetClientWebRtcStatus(std::string_view ClientId) const {
  return m_ClientRegistry->GetWebRtcStatus(ClientId);
}

std::vector<IWebRtcSession *> RemoteViewportServer::CollectClientWebRtcSessions() const {
  return m_ClientRegistry->CollectWebRtcSessions();
}

RemoteViewportServer::ClientSessionResolution
RemoteViewportServer::CreateOrResumeClientSession(
    const std::optional<std::string> &ClientIdHint) {
  return m_ClientRegistry->CreateOrResume(ClientIdHint);
}

void RemoteViewportServer::TouchClientSession(const std::string &ClientId) {
  m_ClientRegistry->Touch(ClientId);
}

std::vector<Project::ProjectDescriptor> RemoteViewportServer::ListProjects() const {
  return m_ProjectWorkspace->ListProjects();
}

std::optional<Project::ProjectDescriptor>
RemoteViewportServer::GetActiveProject() const {
  return m_ProjectWorkspace->GetActiveProject();
}

std::optional<Project::ProjectDescriptor>
RemoteViewportServer::SetActiveProjectBySlug(std::string_view ProjectSlug) {
  return m_ProjectWorkspace->SetActiveProjectBySlug(ProjectSlug);
}

std::filesystem::path RemoteViewportServer::GetActiveContentDir() const {
  return m_ProjectWorkspace->GetActiveContentDir();
}

std::filesystem::path RemoteViewportServer::GetActiveScriptsDir() const {
  return m_ProjectWorkspace->GetActiveScriptsDir();
}

std::filesystem::path RemoteViewportServer::GetEngineContentDir() const {
  return m_ProjectWorkspace->GetEngineContentDir();
}

bool RemoteViewportServer::LoadActiveProjectIntoSession(
    std::string *FailureReason) {
  return m_ProjectWorkspace->LoadActiveProjectIntoSession(FailureReason);
}

std::vector<std::string> RemoteViewportServer::ListScriptFiles() const {
  return m_ProjectWorkspace->ListScriptFiles();
}

std::vector<std::pair<std::string, std::string>>
RemoteViewportServer::ListScriptClasses() const {
  return m_ProjectWorkspace->ListScriptClasses();
}

std::optional<std::filesystem::path>
RemoteViewportServer::ResolveActiveScriptPath(std::string_view RelativePath,
                                              bool AllowMissingLeaf) const {
  return m_ProjectWorkspace->ResolveActiveScriptPath(RelativePath,
                                                     AllowMissingLeaf);
}

std::vector<Assets::AssetDescriptor>
RemoteViewportServer::CollectVisibleAssets() const {
  return m_AssetLibrary->CollectVisibleAssets();
}

std::optional<std::filesystem::path>
RemoteViewportServer::ResolveVisibleAssetPath(std::string_view RelativePath) const {
  return m_AssetLibrary->ResolveVisibleAssetPath(RelativePath);
}

void RemoteViewportServer::HandleClientEncodedVideoPacket(
    std::string_view ClientId, const EncodedVideoPacket &Packet) {
  if (auto Client = FindClientSession(ClientId);
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

void RemoteViewportServer::HandlePlaceActorCommand(SessionUserId User,
                                                    const HeadlessCommand &Command) {
  const EditorSession &Session = m_Host.GetHeadlessLayer().GetSession();
  const EditorViewportState *Viewport = Session.FindViewport(User);
  if (Viewport == nullptr) {
    return;
  }

  glm::vec2 MousePos = Command.MousePosition;
  if (MousePos.x < 0.0f || MousePos.y < 0.0f) {
    MousePos = {static_cast<float>(m_Options.Width) * 0.5f,
                static_cast<float>(m_Options.Height) * 0.5f};
  }

  const glm::vec3 SpawnLocation = ResolveViewportDropPosition(
      Viewport->Camera, m_Options.Width, m_Options.Height, MousePos,
      Session.GetState().Scene.MeshInstances);

  m_Host.SubmitRemoteCommand(
      User, EditorCommand{PlaceActorCommand{
                .ChildTemplateId = Command.PlaceActorTemplateId,
                .ChildMeshAssetPath = Command.PlaceActorMeshAssetPath,
                .Location = SpawnLocation,
            }});
}

bool RemoteViewportServer::BrowserCommandRouter::HandleWebSocketMessage(
    uintptr_t ClientSocketValue, std::string_view Payload) {
  m_Server.m_TotalWebSocketMessages.fetch_add(1);
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
    m_Server.HandleMeshDropCommand(
        m_Server.m_Host.GetHeadlessLayer().GetLocalUserId(), *Command);
    return true;
  case HeadlessCommandType::DropTexture:
    m_Server.HandleTextureDropCommand(
        m_Server.m_Host.GetHeadlessLayer().GetLocalUserId(), *Command);
    return true;
  case HeadlessCommandType::PlaceActor:
    m_Server.HandlePlaceActorCommand(
        m_Server.m_Host.GetHeadlessLayer().GetLocalUserId(), *Command);
    return true;
  case HeadlessCommandType::SetLookActive:
  case HeadlessCommandType::SetViewportCameraPose:
  case HeadlessCommandType::SetCameraProjection:
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
  case HeadlessCommandType::ListAssets:
    m_Server.SendTextMessage(
        ClientSocketValue,
        SerializeAssetList(m_Assets.CollectVisibleAssets()));
    return true;
  case HeadlessCommandType::GetSchema: {
    const auto &DetailsById = m_Server.m_Host.GetHeadlessLayer()
                                  .GetSession()
                                  .GetState()
                                  .Scene.ObjectDetailsById;
    const auto It = DetailsById.find(Command->ObjectId);
    if (It != DetailsById.end()) {
      m_Server.SendTextMessage(ClientSocketValue, SerializeObjectSchema(It->second));
    }
    return true;
  }
  case HeadlessCommandType::SetProperty:
    return false;
  case HeadlessCommandType::SaveScene: {
    const Assets::LocalAssetSource ContentDir{m_Workspace.GetActiveContentDir()};
    const auto ScenePath = ContentDir.ResolveRelative("scene.json");
    const bool Ok = Assets::SaveSceneToFile(
        ScenePath, m_Server.m_Host.GetHeadlessLayer().GetSession().GetState().Scene);
    m_Server.SendTextMessage(ClientSocketValue, SerializeSaveResult(Ok));
    return true;
  }
  case HeadlessCommandType::Quit:
    m_Server.m_StopRequested.store(true);
    m_Server.m_Host.RequestClose();
    m_Server.BroadcastTextMessage(SerializeShutdown());
    return true;
  case HeadlessCommandType::LoadStartupScene:
  case HeadlessCommandType::RenderFrame:
    return false;
  }

  return false;
}

bool RemoteViewportServer::HandleWebSocketMessage(uintptr_t ClientSocketValue,
                                                  std::string_view Payload) {
  return m_CommandRouter->HandleWebSocketMessage(ClientSocketValue, Payload);
}

bool RemoteViewportServer::BrowserCommandRouter::HandleClientWebRtcMessage(
    std::string_view ClientId, std::string_view Payload) {
  m_Server.m_TotalWebSocketMessages.fetch_add(1);
  std::string Error;
  const auto Command = ParseRemoteViewportCommand(Payload, Error);
  if (!Command.has_value()) {
    return false;
  }

  auto Client = m_Registry.Find(ClientId);
  if (Client == nullptr) {
    return false;
  }
  m_Registry.Touch(Client->ClientId);

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
    m_Server.HandleMeshDropCommand(Client->User, *Command);
    return true;
  case HeadlessCommandType::PlaceActor:
    m_Server.HandlePlaceActorCommand(Client->User, *Command);
    return true;

  case HeadlessCommandType::ReloadScripts: {
    m_Server.m_Host.ReloadUserScripts();
    if (Client->WebRtcSession != nullptr) {
      Client->WebRtcSession->SendReliableMessage(
          SerializeTypeOnlyJson("scripts_reloaded"));
    }
    return true;
  }
  case HeadlessCommandType::Heartbeat: {
    const EditorUserPresence *Presence =
        m_Server.m_Host.GetHeadlessLayer().GetSession().FindPresence(Client->User);
    if (Presence != nullptr && Presence->State == EditorUserPresenceState::Away) {
      m_Server.m_Host.GetHeadlessLayer().GetSession().SetPresenceState(
          Client->User, EditorUserPresenceState::Connected);
    }
    return true;
  }
  case HeadlessCommandType::ListAssets: {
    if (Client->WebRtcSession != nullptr) {
      Client->WebRtcSession->SendReliableMessage(
          SerializeAssetList(m_Assets.CollectVisibleAssets()));
    }
    return true;
  }
  case HeadlessCommandType::GetSchema: {
    const auto &DetailsById =
        m_Server.m_Host.GetHeadlessLayer().GetSession().GetState().Scene.ObjectDetailsById;
    const auto It = DetailsById.find(Command->ObjectId);
    if (It != DetailsById.end() && Client->WebRtcSession != nullptr) {
      Client->WebRtcSession->SendReliableMessage(
          SerializeObjectSchema(It->second));
    }
    return true;
  }
  case HeadlessCommandType::SaveScene: {
    const Assets::LocalAssetSource ContentDir{m_Workspace.GetActiveContentDir()};
    const auto ScenePath = ContentDir.ResolveRelative("scene.json");
    const bool Ok = Assets::SaveSceneToFile(
        ScenePath,
        m_Server.m_Host.GetHeadlessLayer().GetSession().GetState().Scene);
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
        m_Server.m_Host.SubmitRemoteCommand(
            Client->User,
            EditorCommand{RenameObjectCommand{.ObjectId = ObjId, .DisplayName = *S}});
        return true;
      }
    } else if (Name == "visible") {
      if (const auto *B = std::get_if<bool>(&Val)) {
        m_Server.m_Host.SubmitRemoteCommand(
            Client->User,
            EditorCommand{SetObjectVisibilityCommand{.ObjectId = ObjId, .Visible = *B}});
        return true;
      }
    } else if (Name == "scriptClass") {
      if (const auto *S = std::get_if<std::string>(&Val)) {
        if (S->empty()) {
          m_Server.m_Host.SubmitRemoteCommand(
              Client->User,
              EditorCommand{DetachScriptCommand{.ObjectId = ObjId}});
        } else {
          m_Server.m_Host.SubmitRemoteCommand(
              Client->User,
              EditorCommand{AttachScriptCommand{.ObjectId = ObjId,
                                               .ScriptClassName = *S}});
        }
        return true;
      }
    } else if (Name == "physicsBodyType" || Name == "physicsColliderType" ||
               Name == "physicsBoxHalfExtents" || Name == "physicsSphereRadius" ||
               Name == "physicsMass" || Name == "physicsFriction" ||
               Name == "physicsRestitution") {
      const auto &DetailsById =
          m_Server.m_Host.GetHeadlessLayer().GetSession().GetState().Scene.ObjectDetailsById;
      const auto It = DetailsById.find(ObjId);
      if (It == DetailsById.end() || !It->second.SupportsTransform) {
        return false;
      }

      Axiom::EditorPhysicsProperties Physics =
          It->second.Physics.value_or(Axiom::EditorPhysicsProperties{});
      if (Name == "physicsBodyType") {
        const auto *S = std::get_if<std::string>(&Val);
        if (S == nullptr) {
          return false;
        }
        if (*S == "none") {
          Physics.BodyType = Axiom::EditorPhysicsBodyType::None;
        } else if (*S == "static") {
          Physics.BodyType = Axiom::EditorPhysicsBodyType::Static;
        } else if (*S == "dynamic") {
          Physics.BodyType = Axiom::EditorPhysicsBodyType::Dynamic;
        } else {
          return false;
        }
      } else if (Name == "physicsColliderType") {
        const auto *S = std::get_if<std::string>(&Val);
        if (S == nullptr) {
          return false;
        }
        if (*S == "none") {
          Physics.ColliderType = Axiom::EditorPhysicsColliderType::None;
        } else if (*S == "box") {
          Physics.ColliderType = Axiom::EditorPhysicsColliderType::Box;
        } else if (*S == "sphere") {
          Physics.ColliderType = Axiom::EditorPhysicsColliderType::Sphere;
        } else {
          return false;
        }
      } else if (Name == "physicsBoxHalfExtents") {
        const auto *V = std::get_if<glm::vec3>(&Val);
        if (V == nullptr) {
          return false;
        }
        Physics.BoxHalfExtents = *V;
      } else if (Name == "physicsSphereRadius") {
        const auto *Number = std::get_if<float>(&Val);
        if (Number == nullptr) {
          return false;
        }
        Physics.SphereRadius = *Number;
      } else if (Name == "physicsMass") {
        const auto *Number = std::get_if<float>(&Val);
        if (Number == nullptr) {
          return false;
        }
        Physics.Mass = *Number;
      } else if (Name == "physicsFriction") {
        const auto *Number = std::get_if<float>(&Val);
        if (Number == nullptr) {
          return false;
        }
        Physics.Friction = *Number;
      } else if (Name == "physicsRestitution") {
        const auto *Number = std::get_if<float>(&Val);
        if (Number == nullptr) {
          return false;
        }
        Physics.Restitution = *Number;
      }

      m_Server.m_Host.SubmitRemoteCommand(
          Client->User,
          EditorCommand{SetPhysicsPropertiesCommand{
              .ObjectId = ObjId,
              .Physics = Physics,
          }});
      return true;
    } else if (Name == "location" || Name == "rotationDegrees" || Name == "scale") {
      if (const auto *V = std::get_if<glm::vec3>(&Val)) {
        const auto &DetailsById =
            m_Server.m_Host.GetHeadlessLayer().GetSession().GetState().Scene.ObjectDetailsById;
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
        m_Server.m_Host.SubmitRemoteCommand(Client->User, EditorCommand{Cmd});
        return true;
      }
    }
    return false;
  }
  case HeadlessCommandType::SetGizmoMode:
    Client->CurrentGizmoMode = Command->Mode;
    m_Server.m_Host.GetHeadlessLayer().SetGizmoMode(Client->User, Command->Mode);
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
    if (m_Server.m_Host.GetHeadlessLayer().GetSession().GetRuntimeState() !=
        EditorRuntimeState::Edit) {
      m_Server.m_Host.GetHeadlessLayer().SetGizmoHoveredAxis(Client->User, -1);
      return true;
    }
    if (Client->GizmoDrag.has_value()) {
      return true;
    }
    const EditorSession &Session = m_Server.m_Host.GetHeadlessLayer().GetSession();
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
          m_Server.m_Options.Width, m_Server.m_Options.Height);
      const int Axis =
          (Client->CurrentGizmoMode == GizmoMode::Rotate)
              ? HitTestGizmoRings(Viewport->Camera, m_Server.m_Options.Width,
                                  m_Server.m_Options.Height, Command->MousePosition,
                                  HoverTD->Location, GizmoScale)
              : HitTestGizmoAxes(Viewport->Camera, m_Server.m_Options.Width,
                                 m_Server.m_Options.Height, Command->MousePosition,
                                 HoverTD->Location, GizmoScale);
          m_Server.m_Host.GetHeadlessLayer().SetGizmoHoveredAxis(Client->User, Axis);
    } else {
      m_Server.m_Host.GetHeadlessLayer().SetGizmoHoveredAxis(Client->User, -1);
    }
    return true;
  }
  case HeadlessCommandType::GizmoDragStart: {
    if (m_Server.m_Host.GetHeadlessLayer().GetSession().GetRuntimeState() !=
        EditorRuntimeState::Edit) {
      return true;
    }
    if (Client->GizmoDrag.has_value()) {
      return true;
    }
    EditorSession &Session = m_Server.m_Host.GetHeadlessLayer().GetSession();
    const EditorViewportState *Viewport =
        Session.FindViewport(Client->User);
    if (Viewport == nullptr) {
      return true;
    }
    const EditorObjectDetails *Selected =
        Session.FindSelectedObjectDetails(Client->User);
    const auto *DragTD =
        (Selected != nullptr && Selected->SupportsTransform &&
         !Selected->TransformReadOnly)
        ? (Selected->WorldTransform.has_value() ? &*Selected->WorldTransform
               : (Selected->Transform.has_value() ? &*Selected->Transform : nullptr))
        : nullptr;

    if (DragTD != nullptr) {
      const glm::vec3 &ObjPos = DragTD->Location;
      const float GizmoScale = ComputeGizmoScale(
          Viewport->Camera, ObjPos, m_Server.m_Options.Width,
          m_Server.m_Options.Height);
      if (Client->CurrentGizmoMode == GizmoMode::Rotate) {
        auto DragState = BeginGizmoRotateDrag(
            Viewport->Camera, m_Server.m_Options.Width, m_Server.m_Options.Height,
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
          m_Server.m_Host.GetHeadlessLayer().SetGizmoHoveredAxis(Client->User,
                                                                 DragState->Axis);
          return true;
        }
      } else {
        auto DragState = BeginGizmoDrag(
            Viewport->Camera, m_Server.m_Options.Width, m_Server.m_Options.Height,
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
          m_Server.m_Host.GetHeadlessLayer().SetGizmoHoveredAxis(Client->User,
                                                                 DragState->Axis);
          return true;
        }
      }
    }

    // No gizmo hit — fall back to viewport object picking.
    const auto Hit = ResolveViewportSelectionHit(
        Viewport->Camera, m_Server.m_Options.Width, m_Server.m_Options.Height,
        Command->MousePosition, Session.GetState().Scene.MeshInstances,
        m_Server.m_Host.GetHeadlessLayer().BuildLightBillboards());
    if (Hit.has_value() && !Hit->ObjectId.empty()) {
      // Multi-instance mesh assets expand into read-only generated children
      // (one per sub-mesh) that share the parent's transform. Picking one of
      // these visually selects the asset, but transforms can only be applied
      // to the movable root — so promote the selection to the root.
      std::string SelectId = Hit->ObjectId;
      if (const EditorObjectDetails *Picked =
              Session.FindObjectDetails(SelectId);
          Picked != nullptr && Picked->IsGeneratedAssetChild &&
          Picked->GeneratedFromAssetRootId.has_value()) {
        SelectId = *Picked->GeneratedFromAssetRootId;
      }
      m_Server.m_Host.SubmitRemoteCommand(Client->User,
          EditorCommand{SelectObjectCommand{.ObjectId = SelectId}});
    }
    return true;
  }
  case HeadlessCommandType::DropTexture: {
    m_Server.HandleTextureDropCommand(Client->User, *Command);
    return true;
  }
  case HeadlessCommandType::GizmoDragUpdate: {
    if (m_Server.m_Host.GetHeadlessLayer().GetSession().GetRuntimeState() !=
        EditorRuntimeState::Edit) {
      if (Client->GizmoDrag.has_value()) {
        EditorSession &Session = m_Server.m_Host.GetHeadlessLayer().GetSession();
        const std::string DragObjectId = Client->GizmoDrag->ObjectId;
        Client->GizmoDrag.reset();
        Session.ReleaseLock(DragObjectId, Client->User);
        m_Server.m_Host.GetHeadlessLayer().SetGizmoHoveredAxis(Client->User, -1);
      }
      return true;
    }
    if (!Client->GizmoDrag.has_value()) {
      return true;
    }
    const EditorSession &Session = m_Server.m_Host.GetHeadlessLayer().GetSession();
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
                                 m_Server.m_Options.Width, m_Server.m_Options.Height,
                                 Command->MousePosition.x, Command->MousePosition.y);
    } else if (Drag.Mode == GizmoMode::Scale) {
      const glm::vec3 NewPosTmp =
          UpdateGizmoDrag(Drag.Math, Viewport->Camera, m_Server.m_Options.Width,
                          m_Server.m_Options.Height, Command->MousePosition.x,
                          Command->MousePosition.y);
      const float DeltaT =
          glm::dot(NewPosTmp - Drag.Math.ObjectStartPos, Drag.Math.AxisDir);
      const float Factor = std::max(
          0.001f, 1.0f + DeltaT / std::max(0.001f, Drag.GizmoScaleAtDragStart));
      Scale[Drag.Math.Axis] = Drag.StartScale[Drag.Math.Axis] * Factor;
    } else {
      const float DeltaDeg = UpdateGizmoRotateDrag(
          Drag.Math, Viewport->Camera, m_Server.m_Options.Width,
          m_Server.m_Options.Height,
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
    m_Server.m_Host.SubmitRemoteCommand(Client->User, Cmd);
    return true;
  }
  case HeadlessCommandType::GizmoDragEnd: {
    if (!Client->GizmoDrag.has_value()) {
      return true;
    }
    EditorSession &Session = m_Server.m_Host.GetHeadlessLayer().GetSession();
    const EditorViewportState *Viewport =
        Session.FindViewport(Client->User);
    if (Viewport != nullptr) {
      const ActiveGizmoDrag &Drag = *Client->GizmoDrag;
      glm::vec3 Location = Drag.Math.ObjectStartPos;
      glm::vec3 RotDeg = Drag.StartRotDeg;
      glm::vec3 Scale = Drag.StartScale;
      if (Drag.Mode == GizmoMode::Translate) {
        Location = UpdateGizmoDrag(Drag.Math, Viewport->Camera,
                                   m_Server.m_Options.Width,
                                   m_Server.m_Options.Height,
                                   Command->MousePosition.x, Command->MousePosition.y);
      } else if (Drag.Mode == GizmoMode::Scale) {
        const glm::vec3 NewPosTmp =
            UpdateGizmoDrag(Drag.Math, Viewport->Camera,
                            m_Server.m_Options.Width,
                            m_Server.m_Options.Height, Command->MousePosition.x,
                            Command->MousePosition.y);
        const float DeltaT =
            glm::dot(NewPosTmp - Drag.Math.ObjectStartPos, Drag.Math.AxisDir);
        const float Factor = std::max(
            0.001f, 1.0f + DeltaT / std::max(0.001f, Drag.GizmoScaleAtDragStart));
        Scale[Drag.Math.Axis] = Drag.StartScale[Drag.Math.Axis] * Factor;
      } else {
        const float DeltaDeg = UpdateGizmoRotateDrag(
            Drag.Math, Viewport->Camera, m_Server.m_Options.Width,
            m_Server.m_Options.Height,
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
      m_Server.m_Host.SubmitRemoteCommand(Client->User, Cmd);
    }
    const std::string DragObjectId = Client->GizmoDrag->ObjectId;
    Client->GizmoDrag.reset();
    Session.ReleaseLock(DragObjectId, Client->User);
    m_Server.m_Host.GetHeadlessLayer().SetGizmoHoveredAxis(Client->User, -1);
    return true;
  }
  case HeadlessCommandType::Quit:
    m_Server.m_StopRequested.store(true);
    m_Server.m_Host.RequestClose();
    m_Server.BroadcastTextMessage(SerializeShutdown());
    return true;
  case HeadlessCommandType::LoadStartupScene:
  case HeadlessCommandType::RenderFrame:
    return false;
  }

  return false;
}

bool RemoteViewportServer::HandleClientWebRtcMessage(std::string_view ClientId,
                                                     std::string_view Payload) {
  return m_CommandRouter->HandleClientWebRtcMessage(ClientId, Payload);
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
