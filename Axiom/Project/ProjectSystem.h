#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Axiom::Project {

struct ProjectManifest {
  std::uint32_t Version{2};
  std::string ProjectId;
  std::string Name;
  std::string Slug;
  std::string ScriptAssemblyName;
  std::string ScriptRootNamespace;
};

struct ProjectRoot {
  std::filesystem::path RootPath;
  std::filesystem::path ManifestPath;
  std::filesystem::path ContentDir;
  std::filesystem::path SceneFilePath;
};

struct ProjectScriptWorkspace {
  std::filesystem::path ScriptsDir;
  std::filesystem::path ScriptProjectPath;
  std::filesystem::path ScriptSolutionPath;
  std::filesystem::path StarterScriptPath;
  std::string AssemblyName;
  std::string RootNamespace;
  std::string StarterScriptClassName;
  std::string StarterScriptQualifiedClassName;
};

struct ProjectOutputLayout {
  std::filesystem::path CookedDir;
  std::filesystem::path CookManifestPath;
  std::filesystem::path BuildDir;
  std::filesystem::path PackageDir;
  std::filesystem::path PackagedContentDir;
  std::filesystem::path PackagedCookedDir;
  std::filesystem::path PackagedCookManifestPath;
  std::filesystem::path PackagedSceneFilePath;
  std::filesystem::path PackagedEngineContentDir;
  std::filesystem::path PackageManifestPath;
};

struct ProjectCookResult {
  ProjectOutputLayout Output;
  std::size_t CookedSourceAssetCount{0};
  std::size_t ManifestEntryCount{0};
};

struct ProjectPackageResult {
  ProjectCookResult Cook;
  std::size_t PackagedFileCount{0};
  bool IncludedSceneFile{false};
  bool IncludedEngineContent{false};
};

struct ProjectDescriptor {
  ProjectManifest Manifest;
  ProjectRoot Root;
  ProjectScriptWorkspace ScriptWorkspace;
  ProjectOutputLayout Output;
};

std::filesystem::path GetDefaultProjectsRoot();
ProjectRoot ResolveProjectRoot(const std::filesystem::path &RootPath);
ProjectScriptWorkspace ResolveProjectScriptWorkspace(const ProjectRoot &Root,
                                                     const ProjectManifest &Manifest);
ProjectOutputLayout ResolveProjectOutputLayout(const ProjectRoot &Root);

bool IsPathWithinRoot(const std::filesystem::path &RootPath,
                      const std::filesystem::path &CandidatePath);
bool IsValidProjectSlug(std::string_view Slug);
std::string SlugifyProjectName(std::string_view Name);
std::string BuildScriptAssemblyName(std::string_view ProjectName);
std::string BuildScriptRootNamespace(std::string_view ProjectName);

bool SaveProjectManifest(const std::filesystem::path &ManifestPath,
                         const ProjectManifest &Manifest);
std::optional<ProjectManifest>
LoadProjectManifest(const std::filesystem::path &ManifestPath);

bool SaveDefaultSceneFile(const std::filesystem::path &SceneFilePath);

std::optional<ProjectDescriptor>
LoadProjectDescriptor(const std::filesystem::path &RootPath);
std::vector<ProjectDescriptor>
DiscoverProjects(const std::filesystem::path &ProjectsRoot);
std::optional<ProjectDescriptor>
OpenProjectBySlug(const std::filesystem::path &ProjectsRoot,
                  std::string_view ProjectSlug);

std::optional<ProjectDescriptor>
CreateProjectScaffold(const std::filesystem::path &ProjectsRoot,
                      std::string_view ProjectName,
                      std::string *FailureReason = nullptr);

std::optional<ProjectCookResult>
CookProjectContent(const ProjectDescriptor &Project,
                   std::string *FailureReason = nullptr);

std::optional<ProjectPackageResult>
PackageProjectContent(const ProjectDescriptor &Project,
                      std::string *FailureReason = nullptr);

} // namespace Axiom::Project
