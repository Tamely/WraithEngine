#include "RemoteViewportHttpRouter.h"

#include "HeadlessCommandProtocol.h"
#include "RemoteViewportServer.h"
#include "RemoteViewportWebRtcSessionManager.h"
#include "RemoteViewportWebSocketDispatch.h"

#ifndef AXIOM_CONTENT_DIR
#define AXIOM_CONTENT_DIR "Content"
#endif

#include <App.h>
#include <HttpParser.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <regex>
#include <sstream>

namespace Axiom {
#include "RemoteViewportHttpRouterHelpers.inl"

RemoteViewportHttpRouter::RemoteViewportHttpRouter(RemoteViewportServer &Server)
    : m_Server(Server), m_ProjectsRoot(Project::GetDefaultProjectsRoot()) {}

uint64_t RemoteViewportHttpRouter::GetTotalHttpRequests() const {
  return m_TotalHttpRequests.load();
}

uintptr_t RemoteViewportHttpRouter::AllocateConnectionId() {
  return m_NextClientConnectionId.fetch_add(1);
}

void RemoteViewportHttpRouter::IncrementRequestCount() {
  m_TotalHttpRequests.fetch_add(1);
}

void RemoteViewportHttpRouter::RegisterPendingResponse(
    uintptr_t ClientSocketValue, void *Response) {
  std::scoped_lock Lock(m_HttpResponseMutex);
  m_PendingHttpResponses[ClientSocketValue] = PendingHttpResponse{
      .Response = Response,
      .Aborted = false,
  };
}

void RemoteViewportHttpRouter::MarkPendingResponseAborted(
    uintptr_t ClientSocketValue) {
  std::scoped_lock Lock(m_HttpResponseMutex);
  auto It = m_PendingHttpResponses.find(ClientSocketValue);
  if (It != m_PendingHttpResponses.end()) {
    It->second.Aborted = true;
    m_PendingHttpResponses.erase(It);
  }
}

bool RemoteViewportHttpRouter::SendHttpResponse(uintptr_t ClientSocketValue,
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

  auto *HttpResponse = static_cast<uWS::HttpResponse<false> *>(Pending.Response);
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

bool RemoteViewportHttpRouter::SendJsonResponse(uintptr_t ClientSocketValue,
                                                std::string_view Status,
                                                std::string_view Payload) {
  return SendHttpResponse(ClientSocketValue, JsonResponse(Status, Payload));
}

bool RemoteViewportHttpRouter::SendJsonError(uintptr_t ClientSocketValue,
                                             std::string_view Status,
                                             std::string_view ErrorMessage) {
  return SendJsonResponse(ClientSocketValue, Status, SerializeError(ErrorMessage));
}

std::vector<Project::ProjectDescriptor> RemoteViewportHttpRouter::ListProjects() const {
  return Project::DiscoverProjects(m_ProjectsRoot);
}

std::optional<Project::ProjectDescriptor>
RemoteViewportHttpRouter::GetActiveProject() const {
  std::scoped_lock Lock(m_ProjectMutex);
  return m_ActiveProject;
}

std::optional<Project::ProjectDescriptor>
RemoteViewportHttpRouter::SetActiveProjectBySlug(std::string_view ProjectSlug) {
  const auto Opened = Project::OpenProjectBySlug(m_ProjectsRoot, ProjectSlug);
  if (!Opened.has_value()) {
    return std::nullopt;
  }

  std::scoped_lock Lock(m_ProjectMutex);
  m_ActiveProject = *Opened;
  return Opened;
}

void RemoteViewportHttpRouter::SetActiveProject(
    const Project::ProjectDescriptor &Project) {
  std::scoped_lock Lock(m_ProjectMutex);
  m_ActiveProject = Project;
}

std::filesystem::path RemoteViewportHttpRouter::GetActiveContentDir() const {
  if (const auto ActiveProject = GetActiveProject(); ActiveProject.has_value()) {
    return ActiveProject->Root.ContentDir;
  }
  return std::filesystem::path(AXIOM_CONTENT_DIR);
}

std::filesystem::path RemoteViewportHttpRouter::GetActiveScriptsDir() const {
  if (const auto ActiveProject = GetActiveProject(); ActiveProject.has_value()) {
    return ActiveProject->ScriptWorkspace.ScriptsDir;
  }
  return std::filesystem::path(AXIOM_PROJECTS_DIR) / "__default__" / "Scripts";
}

std::filesystem::path RemoteViewportHttpRouter::GetEngineContentDir() const {
  return std::filesystem::path(AXIOM_CONTENT_DIR) / "Engine";
}

bool RemoteViewportHttpRouter::LoadActiveProjectIntoSession(
    std::string *FailureReason) {
  if (m_Server.m_Host.LoadStartupSceneIntoSession(GetActiveContentDir())) {
    return true;
  }
  if (FailureReason != nullptr) {
    *FailureReason =
        "Failed to load the active project's startup scene into the session.";
  }
  return false;
}

std::vector<std::string> RemoteViewportHttpRouter::ListScriptFiles() const {
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
    const auto Relative =
        std::filesystem::relative(Entry.path(), ScriptsDir, Error);
    if (!Error) {
      Results.push_back(Relative.generic_string());
    }
  }
  std::sort(Results.begin(), Results.end());
  return Results;
}

std::vector<std::pair<std::string, std::string>>
RemoteViewportHttpRouter::ListScriptClasses() const {
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
RemoteViewportHttpRouter::ResolveActiveScriptPath(std::string_view RelativePath,
                                                  bool AllowMissingLeaf) const {
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

std::vector<Assets::AssetDescriptor>
RemoteViewportHttpRouter::CollectVisibleAssets() const {
  std::vector<Assets::AssetDescriptor> Assets;
  const Assets::LocalAssetSource ProjectSource{GetActiveContentDir()};
  Assets = ProjectSource.List();

  const Assets::LocalAssetSource EngineSource{GetEngineContentDir()};
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
RemoteViewportHttpRouter::ResolveVisibleAssetPath(
    std::string_view RelativePath) const {
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

bool RemoteViewportHttpRouter::HandlePostRequest(uintptr_t ClientSocketValue,
                                                 std::string_view Path,
                                                 std::string_view HeaderBlock,
                                                 std::string_view Body) {
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
    return m_Server.m_WebRtcSessions->HandleSessionConnectRequest(
        ClientSocketValue, HeaderBlock, Body);
  }
  if (Route == "/webrtc/offer") {
    return m_Server.m_WebRtcSessions->HandleWebRtcOfferRequest(
        ClientSocketValue, HeaderBlock, Body);
  }
  if (Route == "/webrtc/ice-candidate") {
    return m_Server.m_WebRtcSessions->HandleWebRtcIceCandidateRequest(
        ClientSocketValue, HeaderBlock, Body);
  }
  if (Route == "/webrtc/close") {
    return m_Server.m_WebRtcSessions->HandleWebRtcCloseRequest(
        ClientSocketValue, HeaderBlock, Body);
  }
  if (Route == "/assets/upload") {
    return HandleAssetUploadRequest(ClientSocketValue, Path, HeaderBlock, Body);
  }
  if (Route != "/command") {
    SendJsonError(ClientSocketValue, "404 Not Found", "Unknown POST endpoint.");
    return false;
  }

  const auto ClientId = FindHeaderValue(HeaderBlock, ClientIdHeaderName);
  const auto User = ClientId.has_value()
                        ? m_Server.m_WebRtcSessions->ResolveClientUser(*ClientId)
                        : std::nullopt;
  if (!User.has_value()) {
    SendJsonError(ClientSocketValue, "400 Bad Request",
                  "Missing or unknown X-Axiom-Client-Id.");
    return false;
  }
  if (ClientId.has_value()) {
    m_Server.m_WebRtcSessions->TouchClientSession(*ClientId);
  }

  std::string Error;
  const auto Command = ParseRemoteViewportCommand(Body, Error);
  if (!Command.has_value()) {
    SendJsonError(ClientSocketValue, "400 Bad Request", Error);
    return false;
  }

  switch (Command->Type) {
  case HeadlessCommandType::SetViewMode:
    m_Server.m_Host.SetRemoteViewMode(*User, Command->ViewMode);
    break;
  case HeadlessCommandType::SetShowColliders:
    m_Server.m_Host.SetRemoteShowColliders(*User, Command->ShowColliders);
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
    m_Server.m_Host.SubmitRemoteCommand(*User, Command->EditorPayload);
    break;
  case HeadlessCommandType::DropMesh:
    m_Server.m_GizmoController->HandleMeshDropCommand(*User, *Command);
    break;
  case HeadlessCommandType::DropTexture:
    m_Server.m_GizmoController->HandleTextureDropCommand(*User, *Command);
    break;
  case HeadlessCommandType::PlaceActor:
    m_Server.m_GizmoController->HandlePlaceActorCommand(*User, *Command);
    break;
  case HeadlessCommandType::Quit:
    m_Server.m_StopRequested.store(true);
    m_Server.m_Host.RequestClose();
    m_Server.m_WebSocketDispatch->BroadcastTextMessage(SerializeShutdown());
    break;
  default:
    break;
  }

  SendJsonResponse(ClientSocketValue, "202 Accepted",
                   SerializeTypeOnlyJson("accepted"));
  return false;
}

bool RemoteViewportHttpRouter::HandleGetRequest(uintptr_t ClientSocketValue,
                                                std::string_view Path,
                                                std::string_view HeaderBlock) {
  const std::string_view Route = StripQuery(Path);
  if (Route == "/projects") {
    SendJsonResponse(ClientSocketValue, "200 OK",
                     SerializeProjectList(ListProjects(), GetActiveProject()));
    return false;
  }
  if (Route == "/projects/current") {
    SendJsonResponse(ClientSocketValue, "200 OK",
                     SerializeCurrentProject(GetActiveProject()));
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
    SendJsonResponse(ClientSocketValue, "200 OK",
                     SerializeReady(m_Server.m_Options.Width,
                                    m_Server.m_Options.Height));
    return false;
  }
  if (Route == "/webrtc") {
    const auto ClientId = FindHeaderValue(HeaderBlock, ClientIdHeaderName);
    const WebRtcSessionStatus Status =
        ClientId.has_value()
            ? m_Server.m_WebRtcSessions->GetClientWebRtcStatus(*ClientId)
            : WebRtcSessionStatus{};
    SendJsonResponse(ClientSocketValue, "200 OK",
                     SerializeWebRtcStatus(
                         Status.Enabled, Status.Available, Status.SignalingState,
                         Status.ConnectionState, Status.Detail, Status.SessionId,
                         Status.PendingLocalIceCandidateCount, Status.Video));
    return false;
  }
  if (Route == "/webrtc/ice-candidates") {
    const auto ClientId = FindHeaderValue(HeaderBlock, ClientIdHeaderName);
    std::vector<WebRtcIceCandidate> Candidates;
    if (ClientId.has_value()) {
      if (auto Client = m_Server.m_WebRtcSessions->FindClientSession(*ClientId);
          Client != nullptr && Client->WebRtcSession != nullptr) {
        Candidates = Client->WebRtcSession->TakePendingLocalIceCandidates();
      }
    }
    SendJsonResponse(ClientSocketValue, "200 OK",
                     SerializeWebRtcIceCandidateList(Candidates));
    return false;
  }
  if (Route == "/session") {
    const auto ClientId = FindHeaderValue(HeaderBlock, ClientIdHeaderName);
    const auto User = ClientId.has_value()
                          ? m_Server.m_WebRtcSessions->ResolveClientUser(*ClientId)
                          : std::nullopt;
    if (!User.has_value()) {
      SendJsonError(ClientSocketValue, "400 Bad Request",
                    "Missing or unknown X-Axiom-Client-Id.");
      return false;
    }
    if (ClientId.has_value()) {
      m_Server.m_WebRtcSessions->TouchClientSession(*ClientId);
    }
    const WebRtcSessionStatus Status =
        ClientId.has_value()
            ? m_Server.m_WebRtcSessions->GetClientWebRtcStatus(*ClientId)
            : WebRtcSessionStatus{};
    const bool ShowColliders = [&]() -> bool {
      if (ClientId.has_value()) {
        if (const HeadlessRenderViewState *View =
                m_Server.m_Host.FindRemoteRenderView(*ClientId);
            View != nullptr) {
          return View->ShowColliders;
        }
      }
      if (const HeadlessRenderViewState *View =
              m_Server.m_Host.FindRenderView(*User);
          View != nullptr) {
        return View->ShowColliders;
      }
      return true;
    }();
    SendJsonResponse(ClientSocketValue, "200 OK",
                     SerializeSessionSnapshot(
                         m_Server.m_Host.GetSessionModule().GetSession().GetState(),
                         *User, ShowColliders,
                         m_Server.m_TransportConnected.load(),
                         m_Server.m_TransportConnected.load() ? "connected"
                                                              : "disconnected",
                         Status.ConnectionState));
    return false;
  }
  if (Route == "/assets/thumbnail") {
    const auto RelPath = GetQueryParam(Path, "path");
    if (!RelPath.has_value() || RelPath->empty()) {
      SendJsonError(ClientSocketValue, "400 Bad Request",
                    "Missing 'path' query parameter.");
      return false;
    }
    if (RelPath->find("..") != std::string::npos) {
      SendJsonError(ClientSocketValue, "400 Bad Request", "Invalid path.");
      return false;
    }
    const auto FullPath = ResolveVisibleAssetPath(*RelPath);
    if (!FullPath.has_value()) {
      SendJsonError(ClientSocketValue, "400 Bad Request", "Invalid path.");
      return false;
    }
    const std::vector<uint8_t> Jpeg = MakeThumbnailJpeg(*FullPath);
    if (Jpeg.empty()) {
      SendJsonError(ClientSocketValue, "404 Not Found",
                    "Could not load thumbnail.");
      return false;
    }
    const std::string_view JpegView(reinterpret_cast<const char *>(Jpeg.data()),
                                    Jpeg.size());
    SendHttpResponse(ClientSocketValue,
                     BuildHttpResponse("200 OK", "image/jpeg", JpegView));
    return false;
  }

  SendJsonError(ClientSocketValue, "404 Not Found", "Unknown GET endpoint.");
  return false;
}

#include "RemoteViewportHttpRouterMutations.inl"
} // namespace Axiom
