#include "Project/ProjectSystem.h"

#include "Core/Log.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <fstream>
#include <sstream>
#include <unordered_map>

#ifndef AXIOM_PROJECTS_DIR
#define AXIOM_PROJECTS_DIR "Projects"
#endif

namespace Axiom::Project {
namespace {

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

class JsonObjectParser {
public:
  explicit JsonObjectParser(std::string_view Text) : m_Text(Text) {}

  bool ParseObject(std::unordered_map<std::string, std::string> &Out) {
    SkipWhitespace();
    if (!Consume('{')) {
      return false;
    }

    SkipWhitespace();
    if (Consume('}')) {
      return true;
    }

    while (m_Index < m_Text.size()) {
      const auto Key = ParseString();
      if (!Key.has_value()) {
        return false;
      }

      SkipWhitespace();
      if (!Consume(':')) {
        return false;
      }

      SkipWhitespace();
      std::string Value;
      if (Peek() == '"') {
        const auto Parsed = ParseString();
        if (!Parsed.has_value()) {
          return false;
        }
        Value = *Parsed;
      } else {
        const size_t Start = m_Index;
        while (m_Index < m_Text.size()) {
          const char Character = m_Text[m_Index];
          if (Character == ',' || Character == '}' ||
              std::isspace(static_cast<unsigned char>(Character))) {
            break;
          }
          ++m_Index;
        }
        Value = std::string(m_Text.substr(Start, m_Index - Start));
      }
      Out.emplace(*Key, std::move(Value));

      SkipWhitespace();
      if (Consume('}')) {
        return true;
      }
      if (!Consume(',')) {
        return false;
      }
      SkipWhitespace();
    }

    return false;
  }

private:
  std::optional<std::string> ParseString() {
    if (!Consume('"')) {
      return std::nullopt;
    }

    std::string Result;
    while (m_Index < m_Text.size()) {
      const char Character = m_Text[m_Index++];
      if (Character == '"') {
        return Result;
      }
      if (Character == '\\') {
        if (m_Index >= m_Text.size()) {
          return std::nullopt;
        }
        const char Escaped = m_Text[m_Index++];
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

  void SkipWhitespace() {
    while (m_Index < m_Text.size() &&
           std::isspace(static_cast<unsigned char>(m_Text[m_Index]))) {
      ++m_Index;
    }
  }

  bool Consume(char Expected) {
    if (Peek() != Expected) {
      return false;
    }
    ++m_Index;
    return true;
  }

  char Peek() const {
    if (m_Index >= m_Text.size()) {
      return '\0';
    }
    return m_Text[m_Index];
  }

  std::string_view m_Text;
  size_t m_Index{0};
};

std::string BuildProjectId(std::string_view Slug) {
  // Stable enough for v1 scaffold creation without adding a UUID dependency.
  std::hash<std::string_view> Hasher;
  const auto Value = static_cast<unsigned long long>(Hasher(Slug));
  std::ostringstream Stream;
  Stream << "project-" << std::hex << Value;
  return Stream.str();
}

bool ReadFileToString(const std::filesystem::path &Path, std::string &Out) {
  std::ifstream File(Path);
  if (!File.is_open()) {
    return false;
  }

  Out.assign(std::istreambuf_iterator<char>(File), std::istreambuf_iterator<char>());
  return true;
}

std::optional<std::uint32_t> ParseUint32(std::string_view Value) {
  std::uint32_t Parsed = 0;
  const auto *Begin = Value.data();
  const auto *End = Begin + Value.size();
  const auto Result = std::from_chars(Begin, End, Parsed);
  if (Result.ec != std::errc{} || Result.ptr != End) {
    return std::nullopt;
  }
  return Parsed;
}

} // namespace

std::filesystem::path GetDefaultProjectsRoot() {
  return std::filesystem::path(AXIOM_PROJECTS_DIR);
}

ProjectRoot ResolveProjectRoot(const std::filesystem::path &RootPath) {
  const auto AbsoluteRoot = std::filesystem::absolute(RootPath).lexically_normal();
  return {
      .RootPath = AbsoluteRoot,
      .ManifestPath = AbsoluteRoot / "project.wraith.json",
      .ContentDir = AbsoluteRoot / "Content",
      .SceneFilePath = AbsoluteRoot / "Content" / "scene.json",
  };
}

bool IsPathWithinRoot(const std::filesystem::path &RootPath,
                      const std::filesystem::path &CandidatePath) {
  std::error_code Error;
  const auto CanonicalRoot =
      std::filesystem::weakly_canonical(RootPath, Error).lexically_normal();
  if (Error) {
    return false;
  }

  Error.clear();
  const auto CanonicalCandidate =
      std::filesystem::weakly_canonical(CandidatePath, Error).lexically_normal();
  if (Error) {
    return false;
  }

  auto RootIt = CanonicalRoot.begin();
  auto CandidateIt = CanonicalCandidate.begin();
  for (; RootIt != CanonicalRoot.end() && CandidateIt != CanonicalCandidate.end();
       ++RootIt, ++CandidateIt) {
    if (*RootIt != *CandidateIt) {
      return false;
    }
  }

  return RootIt == CanonicalRoot.end();
}

bool IsValidProjectSlug(std::string_view Slug) {
  if (Slug.empty()) {
    return false;
  }
  if (Slug.front() == '-' || Slug.back() == '-') {
    return false;
  }

  for (const char Character : Slug) {
    if ((Character >= 'a' && Character <= 'z') ||
        (Character >= '0' && Character <= '9') || Character == '-') {
      continue;
    }
    return false;
  }
  return true;
}

std::string SlugifyProjectName(std::string_view Name) {
  std::string Slug;
  Slug.reserve(Name.size());
  bool PreviousWasDash = false;

  for (const char Character : Name) {
    const unsigned char UnsignedCharacter =
        static_cast<unsigned char>(Character);
    if (std::isalnum(UnsignedCharacter)) {
      Slug.push_back(
          static_cast<char>(std::tolower(UnsignedCharacter)));
      PreviousWasDash = false;
      continue;
    }

    if (!Slug.empty() && !PreviousWasDash) {
      Slug.push_back('-');
      PreviousWasDash = true;
    }
  }

  while (!Slug.empty() && Slug.back() == '-') {
    Slug.pop_back();
  }

  return Slug;
}

bool SaveProjectManifest(const std::filesystem::path &ManifestPath,
                         const ProjectManifest &Manifest) {
  std::error_code Error;
  std::filesystem::create_directories(ManifestPath.parent_path(), Error);
  if (Error) {
    A_CORE_ERROR("ProjectSystem: failed to create manifest directory '{}'",
                 ManifestPath.parent_path().string());
    return false;
  }

  std::ofstream File(ManifestPath);
  if (!File.is_open()) {
    A_CORE_ERROR("ProjectSystem: failed to open manifest '{}'",
                 ManifestPath.string());
    return false;
  }

  File << "{\n"
       << "  \"version\": " << Manifest.Version << ",\n"
       << "  \"projectId\": \"" << EscapeJsonString(Manifest.ProjectId) << "\",\n"
       << "  \"name\": \"" << EscapeJsonString(Manifest.Name) << "\",\n"
       << "  \"slug\": \"" << EscapeJsonString(Manifest.Slug) << "\"\n"
       << "}\n";
  return File.good();
}

std::optional<ProjectManifest>
LoadProjectManifest(const std::filesystem::path &ManifestPath) {
  std::string Text;
  if (!ReadFileToString(ManifestPath, Text)) {
    return std::nullopt;
  }

  JsonObjectParser Parser(Text);
  std::unordered_map<std::string, std::string> Fields;
  if (!Parser.ParseObject(Fields)) {
    A_CORE_WARN("ProjectSystem: failed to parse manifest '{}'",
                ManifestPath.string());
    return std::nullopt;
  }

  const auto VersionIt = Fields.find("version");
  const auto ProjectIdIt = Fields.find("projectId");
  const auto NameIt = Fields.find("name");
  const auto SlugIt = Fields.find("slug");
  if (VersionIt == Fields.end() || ProjectIdIt == Fields.end() ||
      NameIt == Fields.end() || SlugIt == Fields.end()) {
    return std::nullopt;
  }

  const auto Version = ParseUint32(VersionIt->second);
  if (!Version.has_value() || !IsValidProjectSlug(SlugIt->second)) {
    return std::nullopt;
  }

  return ProjectManifest{
      .Version = *Version,
      .ProjectId = ProjectIdIt->second,
      .Name = NameIt->second,
      .Slug = SlugIt->second,
  };
}

bool SaveDefaultSceneFile(const std::filesystem::path &SceneFilePath) {
  std::error_code Error;
  std::filesystem::create_directories(SceneFilePath.parent_path(), Error);
  if (Error) {
    A_CORE_ERROR("ProjectSystem: failed to create scene directory '{}'",
                 SceneFilePath.parent_path().string());
    return false;
  }

  std::ofstream File(SceneFilePath);
  if (!File.is_open()) {
    A_CORE_ERROR("ProjectSystem: failed to open scene file '{}'",
                 SceneFilePath.string());
    return false;
  }

  File << "{\n"
       << "  \"version\": 1,\n"
       << "  \"meshAsset\": \"\",\n"
       << "  \"nodes\": [],\n"
       << "  \"objects\": []\n"
       << "}\n";
  return File.good();
}

std::optional<ProjectDescriptor>
LoadProjectDescriptor(const std::filesystem::path &RootPath) {
  const ProjectRoot Root = ResolveProjectRoot(RootPath);
  const auto Manifest = LoadProjectManifest(Root.ManifestPath);
  if (!Manifest.has_value()) {
    return std::nullopt;
  }

  return ProjectDescriptor{
      .Manifest = *Manifest,
      .Root = Root,
  };
}

std::vector<ProjectDescriptor>
DiscoverProjects(const std::filesystem::path &ProjectsRoot) {
  std::vector<ProjectDescriptor> Results;
  if (!std::filesystem::exists(ProjectsRoot)) {
    return Results;
  }

  for (const auto &Entry : std::filesystem::directory_iterator(ProjectsRoot)) {
    if (!Entry.is_directory()) {
      continue;
    }
    if (const auto Descriptor = LoadProjectDescriptor(Entry.path());
        Descriptor.has_value()) {
      Results.push_back(*Descriptor);
    }
  }

  std::sort(Results.begin(), Results.end(),
            [](const ProjectDescriptor &Left, const ProjectDescriptor &Right) {
              return Left.Manifest.Name < Right.Manifest.Name;
            });
  return Results;
}

std::optional<ProjectDescriptor>
CreateProjectScaffold(const std::filesystem::path &ProjectsRoot,
                      std::string_view ProjectName,
                      std::string *FailureReason) {
  const std::string Slug = SlugifyProjectName(ProjectName);
  if (!IsValidProjectSlug(Slug)) {
    if (FailureReason != nullptr) {
      *FailureReason = "Project name must contain at least one letter or number.";
    }
    return std::nullopt;
  }

  std::error_code Error;
  std::filesystem::create_directories(ProjectsRoot, Error);
  if (Error) {
    if (FailureReason != nullptr) {
      *FailureReason = "Failed to create projects root directory.";
    }
    return std::nullopt;
  }

  const ProjectRoot Root = ResolveProjectRoot(ProjectsRoot / Slug);
  if (std::filesystem::exists(Root.RootPath)) {
    if (FailureReason != nullptr) {
      *FailureReason = "A project with that slug already exists.";
    }
    return std::nullopt;
  }

  if (!IsPathWithinRoot(ProjectsRoot, Root.RootPath.parent_path()) &&
      Root.RootPath.parent_path() != std::filesystem::absolute(ProjectsRoot).lexically_normal()) {
    if (FailureReason != nullptr) {
      *FailureReason = "Project root must remain inside the managed projects directory.";
    }
    return std::nullopt;
  }

  std::filesystem::create_directories(Root.ContentDir, Error);
  if (Error) {
    if (FailureReason != nullptr) {
      *FailureReason = "Failed to create project content directory.";
    }
    return std::nullopt;
  }

  const ProjectManifest Manifest{
      .Version = 1,
      .ProjectId = BuildProjectId(Slug),
      .Name = std::string(ProjectName),
      .Slug = Slug,
  };
  if (!SaveProjectManifest(Root.ManifestPath, Manifest) ||
      !SaveDefaultSceneFile(Root.SceneFilePath)) {
    std::filesystem::remove_all(Root.RootPath, Error);
    if (FailureReason != nullptr) {
      *FailureReason = "Failed to write the initial project scaffold.";
    }
    return std::nullopt;
  }

  return ProjectDescriptor{
      .Manifest = Manifest,
      .Root = Root,
  };
}

} // namespace Axiom::Project
