#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Axiom::Project {

struct ProjectManifest {
  std::uint32_t Version{1};
  std::string ProjectId;
  std::string Name;
  std::string Slug;
};

struct ProjectRoot {
  std::filesystem::path RootPath;
  std::filesystem::path ManifestPath;
  std::filesystem::path ContentDir;
  std::filesystem::path SceneFilePath;
};

struct ProjectDescriptor {
  ProjectManifest Manifest;
  ProjectRoot Root;
};

std::filesystem::path GetDefaultProjectsRoot();
ProjectRoot ResolveProjectRoot(const std::filesystem::path &RootPath);

bool IsPathWithinRoot(const std::filesystem::path &RootPath,
                      const std::filesystem::path &CandidatePath);
bool IsValidProjectSlug(std::string_view Slug);
std::string SlugifyProjectName(std::string_view Name);

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
CreateProjectScaffold(const std::filesystem::path &ProjectsRoot,
                      std::string_view ProjectName,
                      std::string *FailureReason = nullptr);

} // namespace Axiom::Project
