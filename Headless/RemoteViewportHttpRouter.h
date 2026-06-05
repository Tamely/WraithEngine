#pragma once

#include <Assets/IAssetSource.h>
#include <Project/ProjectSystem.h>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Axiom {
class RemoteViewportServer;

class RemoteViewportHttpRouter {
public:
  explicit RemoteViewportHttpRouter(RemoteViewportServer &Server);

  uint64_t GetTotalHttpRequests() const;
  uintptr_t AllocateConnectionId();
  void IncrementRequestCount();
  void RegisterPendingResponse(uintptr_t ClientSocketValue, void *Response);
  void MarkPendingResponseAborted(uintptr_t ClientSocketValue);
  bool SendHttpResponse(uintptr_t ClientSocketValue, std::string_view Response);
  bool SendJsonResponse(uintptr_t ClientSocketValue, std::string_view Status,
                        std::string_view Payload);
  bool SendJsonError(uintptr_t ClientSocketValue, std::string_view Status,
                     std::string_view ErrorMessage);

  bool HandleGetRequest(uintptr_t ClientSocketValue, std::string_view Path,
                        std::string_view HeaderBlock);
  bool HandlePostRequest(uintptr_t ClientSocketValue, std::string_view Path,
                         std::string_view HeaderBlock,
                         std::string_view Body);

  std::vector<Project::ProjectDescriptor> ListProjects() const;
  std::optional<Project::ProjectDescriptor> GetActiveProject() const;
  std::optional<Project::ProjectDescriptor>
  SetActiveProjectBySlug(std::string_view ProjectSlug);
  void SetActiveProject(const Project::ProjectDescriptor &Project);
  std::filesystem::path GetActiveContentDir() const;
  std::filesystem::path GetActiveScriptsDir() const;
  std::filesystem::path GetEngineContentDir() const;
  bool LoadActiveProjectIntoSession(std::string *FailureReason = nullptr);
  std::vector<std::string> ListScriptFiles() const;
  std::vector<std::pair<std::string, std::string>> ListScriptClasses() const;
  std::optional<std::filesystem::path>
  ResolveActiveScriptPath(std::string_view RelativePath,
                          bool AllowMissingLeaf = false) const;
  std::vector<Assets::AssetDescriptor> CollectVisibleAssets() const;
  std::optional<std::filesystem::path>
  ResolveVisibleAssetPath(std::string_view RelativePath) const;

private:
  struct PendingHttpResponse {
    void *Response{nullptr};
    bool Aborted{false};
  };

  bool HandleCreateProjectRequest(uintptr_t ClientSocketValue,
                                  std::string_view Body);
  bool HandleOpenProjectRequest(uintptr_t ClientSocketValue,
                                std::string_view Body);
  bool HandleCookProjectRequest(uintptr_t ClientSocketValue);
  bool HandlePackageProjectRequest(uintptr_t ClientSocketValue);
  bool HandleListScriptsRequest(uintptr_t ClientSocketValue);
  bool HandleListScriptClassesRequest(uintptr_t ClientSocketValue);
  bool HandleReadScriptFileRequest(uintptr_t ClientSocketValue,
                                   std::string_view Path);
  bool HandleCreateScriptFileRequest(uintptr_t ClientSocketValue,
                                     std::string_view Body);
  bool HandleSaveScriptFileRequest(uintptr_t ClientSocketValue,
                                   std::string_view Body);
  bool HandleRenameScriptFileRequest(uintptr_t ClientSocketValue,
                                     std::string_view Body);
  bool HandleDeleteScriptFileRequest(uintptr_t ClientSocketValue,
                                     std::string_view Body);
  bool HandleAssetUploadRequest(uintptr_t ClientSocketValue,
                                std::string_view Path,
                                std::string_view HeaderBlock,
                                std::string_view Body);

  RemoteViewportServer &m_Server;
  const std::filesystem::path m_ProjectsRoot;
  mutable std::mutex m_ProjectMutex;
  std::optional<Project::ProjectDescriptor> m_ActiveProject;
  std::atomic<uintptr_t> m_NextClientConnectionId{1};
  mutable std::mutex m_HttpResponseMutex;
  std::unordered_map<uintptr_t, PendingHttpResponse> m_PendingHttpResponses;
  std::atomic<uint64_t> m_TotalHttpRequests{0};
};
} // namespace Axiom
