#include "Project/ProjectSystem.h"

#include "Assets/AssetCookManifest.h"
#include "Assets/AssetCooker.h"
#include "Core/Log.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <unordered_map>

#ifndef AXIOM_PROJECTS_DIR
#define AXIOM_PROJECTS_DIR "Projects"
#endif

#ifndef AXIOM_SOURCE_DIR
#define AXIOM_SOURCE_DIR "."
#endif

#ifndef AXIOM_CONTENT_DIR
#define AXIOM_CONTENT_DIR "Content"
#endif

namespace Axiom::Project {
namespace {
constexpr std::string_view kDefaultStarterScriptClassName = "StarterScript";
constexpr std::string_view kCsProjectTypeGuid =
    "{FAE04EC0-301F-11D3-BF4B-00C04F79EFBC}";

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

std::string BuildIdentifierToken(std::string_view Value) {
  std::string Result;
  Result.reserve(Value.size());
  bool Started = false;
  bool CapitalizeNext = true;

  for (const char Character : Value) {
    const unsigned char UnsignedCharacter =
        static_cast<unsigned char>(Character);
    if (std::isalnum(UnsignedCharacter) == 0) {
      CapitalizeNext = true;
      continue;
    }

    if (!Started) {
      if (std::isdigit(UnsignedCharacter) != 0) {
        Result += "Project";
      }
      Result.push_back(
          static_cast<char>(std::toupper(UnsignedCharacter)));
      Started = true;
      CapitalizeNext = false;
      continue;
    }

    if (CapitalizeNext) {
      Result.push_back(
          static_cast<char>(std::toupper(UnsignedCharacter)));
      CapitalizeNext = false;
    } else {
      Result.push_back(
          static_cast<char>(std::tolower(UnsignedCharacter)));
    }
  }

  if (Result.empty()) {
    return "ProjectScripts";
  }

  return Result;
}

std::string BuildStableGuid(std::string_view Seed) {
  const auto FirstHash =
      static_cast<unsigned long long>(std::hash<std::string_view>{}(Seed));
  const auto SecondHash = static_cast<unsigned long long>(
      std::hash<std::string>{}(std::string(Seed) + "::wraith"));

  std::array<unsigned char, 16> Bytes{};
  for (size_t Index = 0; Index < 8; ++Index) {
    Bytes[Index] =
        static_cast<unsigned char>((FirstHash >> ((7 - Index) * 8)) & 0xffu);
    Bytes[Index + 8] =
        static_cast<unsigned char>((SecondHash >> ((7 - Index) * 8)) & 0xffu);
  }

  Bytes[6] = static_cast<unsigned char>((Bytes[6] & 0x0fu) | 0x40u);
  Bytes[8] = static_cast<unsigned char>((Bytes[8] & 0x3fu) | 0x80u);

  std::ostringstream Stream;
  Stream << std::uppercase << std::hex << std::setfill('0')
         << "{"
         << std::setw(2) << static_cast<int>(Bytes[0])
         << std::setw(2) << static_cast<int>(Bytes[1])
         << std::setw(2) << static_cast<int>(Bytes[2])
         << std::setw(2) << static_cast<int>(Bytes[3])
         << "-"
         << std::setw(2) << static_cast<int>(Bytes[4])
         << std::setw(2) << static_cast<int>(Bytes[5])
         << "-"
         << std::setw(2) << static_cast<int>(Bytes[6])
         << std::setw(2) << static_cast<int>(Bytes[7])
         << "-"
         << std::setw(2) << static_cast<int>(Bytes[8])
         << std::setw(2) << static_cast<int>(Bytes[9])
         << "-"
         << std::setw(2) << static_cast<int>(Bytes[10])
         << std::setw(2) << static_cast<int>(Bytes[11])
         << std::setw(2) << static_cast<int>(Bytes[12])
         << std::setw(2) << static_cast<int>(Bytes[13])
         << std::setw(2) << static_cast<int>(Bytes[14])
         << std::setw(2) << static_cast<int>(Bytes[15])
         << "}";
  return Stream.str();
}

bool WriteTextFile(const std::filesystem::path &Path,
                   std::string_view Contents) {
  std::error_code Error;
  std::filesystem::create_directories(Path.parent_path(), Error);
  if (Error) {
    A_CORE_ERROR("ProjectSystem: failed to create parent directory '{}'",
                 Path.parent_path().string());
    return false;
  }

  std::ofstream File(Path);
  if (!File.is_open()) {
    A_CORE_ERROR("ProjectSystem: failed to open file '{}'", Path.string());
    return false;
  }

  File << Contents;
  return File.good();
}

bool SaveDefaultScriptProject(
    const std::filesystem::path &ProjectPath,
    const ProjectScriptWorkspace &ScriptWorkspace) {
  const auto EngineManagedPath =
      std::filesystem::path(AXIOM_SOURCE_DIR) / "Scripting" /
      "WraithEngine.Managed" / "bin" / "Debug" / "WraithEngine.Managed.dll";

  std::ostringstream Stream;
  Stream << "<Project Sdk=\"Microsoft.NET.Sdk\">\n"
         << "  <PropertyGroup>\n"
         << "    <OutputType>Library</OutputType>\n"
         << "    <AssemblyName>" << ScriptWorkspace.AssemblyName
         << "</AssemblyName>\n"
         << "    <RootNamespace>" << ScriptWorkspace.RootNamespace
         << "</RootNamespace>\n"
         << "    <TargetFramework>net9.0</TargetFramework>\n"
         << "    <ImplicitUsings>enable</ImplicitUsings>\n"
         << "    <Nullable>enable</Nullable>\n"
         << "    <AppendTargetFrameworkToOutputPath>false</AppendTargetFrameworkToOutputPath>\n"
         << "  </PropertyGroup>\n\n"
         << "  <ItemGroup>\n"
         << "    <Reference Include=\"WraithEngine.Managed\">\n"
         << "      <HintPath>" << EngineManagedPath.string()
         << "</HintPath>\n"
         << "      <Private>false</Private>\n"
         << "    </Reference>\n"
         << "  </ItemGroup>\n"
         << "</Project>\n";
  return WriteTextFile(ProjectPath, Stream.str());
}

bool SaveDefaultScriptSolution(
    const std::filesystem::path &SolutionPath,
    const ProjectScriptWorkspace &ScriptWorkspace) {
  const auto ProjectGuid =
      BuildStableGuid(ScriptWorkspace.AssemblyName + "::scripts-project");

  std::ostringstream Stream;
  Stream << "Microsoft Visual Studio Solution File, Format Version 12.00\n"
         << "# Visual Studio Version 17\n"
         << "VisualStudioVersion = 17.0.31903.59\n"
         << "MinimumVisualStudioVersion = 10.0.40219.1\n"
         << "Project(\"" << kCsProjectTypeGuid << "\") = \""
         << ScriptWorkspace.AssemblyName << "\", \"Scripts/"
         << ScriptWorkspace.ScriptProjectPath.filename().string() << "\", \""
         << ProjectGuid << "\"\n"
         << "EndProject\n"
         << "Global\n"
         << "\tGlobalSection(SolutionConfigurationPlatforms) = preSolution\n"
         << "\t\tDebug|Any CPU = Debug|Any CPU\n"
         << "\t\tRelease|Any CPU = Release|Any CPU\n"
         << "\tEndGlobalSection\n"
         << "\tGlobalSection(ProjectConfigurationPlatforms) = postSolution\n"
         << "\t\t" << ProjectGuid
         << ".Debug|Any CPU.ActiveCfg = Debug|Any CPU\n"
         << "\t\t" << ProjectGuid
         << ".Debug|Any CPU.Build.0 = Debug|Any CPU\n"
         << "\t\t" << ProjectGuid
         << ".Release|Any CPU.ActiveCfg = Release|Any CPU\n"
         << "\t\t" << ProjectGuid
         << ".Release|Any CPU.Build.0 = Release|Any CPU\n"
         << "\tEndGlobalSection\n"
         << "\tGlobalSection(SolutionProperties) = preSolution\n"
         << "\t\tHideSolutionNode = FALSE\n"
         << "\tEndGlobalSection\n"
         << "EndGlobal\n";
  return WriteTextFile(SolutionPath, Stream.str());
}

bool SaveDefaultStarterScript(
    const std::filesystem::path &ScriptPath,
    const ProjectScriptWorkspace &ScriptWorkspace) {
  std::ostringstream Stream;
  Stream << "using WraithEngine;\n\n"
         << "namespace " << ScriptWorkspace.RootNamespace << ";\n\n"
         << "public class " << ScriptWorkspace.StarterScriptClassName
         << " : Script\n"
         << "{\n"
         << "    public override void OnCreate()\n"
         << "    {\n"
         << "    }\n\n"
         << "    public override void OnTick(float dt)\n"
         << "    {\n"
         << "    }\n"
         << "}\n";
  return WriteTextFile(ScriptPath, Stream.str());
}

bool EnsureOutputLayoutScaffold(const ProjectOutputLayout &Output) {
  std::error_code Error;
  std::filesystem::create_directories(Output.CookedDir, Error);
  if (Error) {
    return false;
  }
  Error.clear();
  std::filesystem::create_directories(Output.BuildDir, Error);
  if (Error) {
    return false;
  }
  Error.clear();
  std::filesystem::create_directories(Output.PackageDir, Error);
  return !Error;
}

bool EnsureScriptWorkspaceScaffold(ProjectDescriptor &Descriptor) {
  bool ManifestChanged = false;
  if (Descriptor.Manifest.Version < 2) {
    Descriptor.Manifest.Version = 2;
    ManifestChanged = true;
  }
  if (Descriptor.Manifest.ScriptAssemblyName.empty()) {
    Descriptor.Manifest.ScriptAssemblyName =
        BuildScriptAssemblyName(Descriptor.Manifest.Name);
    ManifestChanged = true;
  }
  if (Descriptor.Manifest.ScriptRootNamespace.empty()) {
    Descriptor.Manifest.ScriptRootNamespace =
        BuildScriptRootNamespace(Descriptor.Manifest.Name);
    ManifestChanged = true;
  }

  Descriptor.ScriptWorkspace =
      ResolveProjectScriptWorkspace(Descriptor.Root, Descriptor.Manifest);
  Descriptor.Output = ResolveProjectOutputLayout(Descriptor.Root);

  if (ManifestChanged &&
      !SaveProjectManifest(Descriptor.Root.ManifestPath, Descriptor.Manifest)) {
    return false;
  }

  if (!std::filesystem::exists(Descriptor.ScriptWorkspace.ScriptProjectPath) &&
      !SaveDefaultScriptProject(Descriptor.ScriptWorkspace.ScriptProjectPath,
                                Descriptor.ScriptWorkspace)) {
    return false;
  }
  if (!std::filesystem::exists(Descriptor.ScriptWorkspace.ScriptSolutionPath) &&
      !SaveDefaultScriptSolution(Descriptor.ScriptWorkspace.ScriptSolutionPath,
                                 Descriptor.ScriptWorkspace)) {
    return false;
  }
  if (!std::filesystem::exists(Descriptor.ScriptWorkspace.StarterScriptPath) &&
      !SaveDefaultStarterScript(Descriptor.ScriptWorkspace.StarterScriptPath,
                                Descriptor.ScriptWorkspace)) {
    return false;
  }
  if (!EnsureOutputLayoutScaffold(Descriptor.Output)) {
    return false;
  }

  return true;
}

bool IsCookableContentPath(const std::filesystem::path &RelativePath) {
  const std::string Extension = RelativePath.extension().string();
  return Extension == ".glb" || Extension == ".gltf" || Extension == ".fbx" ||
         Extension == ".obj" || Extension == ".png" || Extension == ".jpg" ||
         Extension == ".jpeg";
}

std::vector<std::filesystem::path>
CollectCookableAssets(const std::filesystem::path &ContentDir) {
  std::vector<std::filesystem::path> Results;
  if (!std::filesystem::exists(ContentDir)) {
    return Results;
  }

  for (const auto &Entry :
       std::filesystem::recursive_directory_iterator(ContentDir)) {
    if (!Entry.is_regular_file()) {
      continue;
    }
    const auto RelativePath =
        std::filesystem::relative(Entry.path(), ContentDir).lexically_normal();
    if (!IsCookableContentPath(RelativePath)) {
      continue;
    }
    Results.push_back(RelativePath);
  }

  std::sort(Results.begin(), Results.end());
  return Results;
}

std::size_t CountPackagedFiles(const std::filesystem::path &RootPath) {
  if (!std::filesystem::exists(RootPath)) {
    return 0;
  }

  std::size_t Count = 0;
  for (const auto &Entry :
       std::filesystem::recursive_directory_iterator(RootPath)) {
    if (Entry.is_regular_file()) {
      ++Count;
    }
  }
  return Count;
}

bool CopyDirectoryTree(const std::filesystem::path &Source,
                       const std::filesystem::path &Destination) {
  if (!std::filesystem::exists(Source)) {
    return true;
  }

  std::error_code Error;
  std::filesystem::create_directories(Destination, Error);
  if (Error) {
    return false;
  }

  for (const auto &Entry : std::filesystem::recursive_directory_iterator(Source)) {
    const auto Relative =
        std::filesystem::relative(Entry.path(), Source).lexically_normal();
    const auto TargetPath = Destination / Relative;
    if (Entry.is_directory()) {
      std::filesystem::create_directories(TargetPath, Error);
      if (Error) {
        return false;
      }
      continue;
    }
    if (!Entry.is_regular_file()) {
      continue;
    }
    std::filesystem::create_directories(TargetPath.parent_path(), Error);
    if (Error) {
      return false;
    }
    std::filesystem::copy_file(Entry.path(), TargetPath,
                               std::filesystem::copy_options::overwrite_existing,
                               Error);
    if (Error) {
      return false;
    }
  }

  return true;
}

bool SavePackageManifestFile(const ProjectDescriptor &Project,
                             const ProjectPackageResult &PackageResult) {
  std::ostringstream Stream;
  Stream << "{\n"
         << "  \"version\": 1,\n"
         << "  \"projectId\": \"" << EscapeJsonString(Project.Manifest.ProjectId)
         << "\",\n"
         << "  \"name\": \"" << EscapeJsonString(Project.Manifest.Name) << "\",\n"
         << "  \"slug\": \"" << EscapeJsonString(Project.Manifest.Slug) << "\",\n"
         << "  \"contentMode\": \"transitional-scene-plus-cooked-assets\",\n"
         << "  \"sceneFile\": \"Content/scene.json\",\n"
         << "  \"cookedDir\": \"Content/Cooked\",\n"
         << "  \"assetCookManifest\": \"Content/Cooked/AssetCookManifest.json\",\n"
         << "  \"engineContentDir\": \"Content/Engine\",\n"
         << "  \"cookedSourceAssetCount\": "
         << PackageResult.Cook.CookedSourceAssetCount << ",\n"
         << "  \"manifestEntryCount\": " << PackageResult.Cook.ManifestEntryCount
         << "\n"
         << "}\n";
  return WriteTextFile(Project.Output.PackageManifestPath, Stream.str());
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

ProjectScriptWorkspace ResolveProjectScriptWorkspace(const ProjectRoot &Root,
                                                     const ProjectManifest &Manifest) {
  const std::string AssemblyName =
      Manifest.ScriptAssemblyName.empty()
          ? BuildScriptAssemblyName(Manifest.Name)
          : Manifest.ScriptAssemblyName;
  const std::string RootNamespace =
      Manifest.ScriptRootNamespace.empty()
          ? BuildScriptRootNamespace(Manifest.Name)
          : Manifest.ScriptRootNamespace;
  const std::string StarterScriptClassName =
      std::string(kDefaultStarterScriptClassName);

  return {
      .ScriptsDir = Root.RootPath / "Scripts",
      .ScriptProjectPath = Root.RootPath / "Scripts" / (AssemblyName + ".csproj"),
      .ScriptSolutionPath = Root.RootPath / (AssemblyName + ".sln"),
      .StarterScriptPath =
          Root.RootPath / "Scripts" / (StarterScriptClassName + ".cs"),
      .AssemblyName = AssemblyName,
      .RootNamespace = RootNamespace,
      .StarterScriptClassName = StarterScriptClassName,
      .StarterScriptQualifiedClassName =
          RootNamespace + "." + StarterScriptClassName,
  };
}

ProjectOutputLayout ResolveProjectOutputLayout(const ProjectRoot &Root) {
  return {
      .CookedDir = Root.ContentDir / "Cooked",
      .CookManifestPath = Root.ContentDir / "Cooked" / "AssetCookManifest.json",
      .BuildDir = Root.RootPath / "Build",
      .PackageDir = Root.RootPath / "Package",
      .PackagedContentDir = Root.RootPath / "Package" / "Content",
      .PackagedCookedDir = Root.RootPath / "Package" / "Content" / "Cooked",
      .PackagedCookManifestPath =
          Root.RootPath / "Package" / "Content" / "Cooked" /
          "AssetCookManifest.json",
      .PackagedSceneFilePath = Root.RootPath / "Package" / "Content" / "scene.json",
      .PackagedEngineContentDir =
          Root.RootPath / "Package" / "Content" / "Engine",
      .PackageManifestPath = Root.RootPath / "Package" / "package.wraith.json",
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

std::string BuildScriptAssemblyName(std::string_view ProjectName) {
  return BuildIdentifierToken(ProjectName) + ".Scripts";
}

std::string BuildScriptRootNamespace(std::string_view ProjectName) {
  return BuildIdentifierToken(ProjectName) + ".Scripts";
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
       << "  \"slug\": \"" << EscapeJsonString(Manifest.Slug) << "\",\n"
       << "  \"scriptAssemblyName\": \""
       << EscapeJsonString(Manifest.ScriptAssemblyName) << "\",\n"
       << "  \"scriptRootNamespace\": \""
       << EscapeJsonString(Manifest.ScriptRootNamespace) << "\"\n"
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
      .ScriptAssemblyName = [&Fields, &NameIt]() {
        const auto ScriptAssemblyIt = Fields.find("scriptAssemblyName");
        return ScriptAssemblyIt != Fields.end()
                   ? ScriptAssemblyIt->second
                   : BuildScriptAssemblyName(NameIt->second);
      }(),
      .ScriptRootNamespace = [&Fields, &NameIt]() {
        const auto ScriptNamespaceIt = Fields.find("scriptRootNamespace");
        return ScriptNamespaceIt != Fields.end()
                   ? ScriptNamespaceIt->second
                   : BuildScriptRootNamespace(NameIt->second);
      }(),
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
       << "  \"nodes\": [\n"
       << "    {\n"
       << "      \"id\": \"world\",\n"
       << "      \"parentId\": null,\n"
       << "      \"displayName\": \"World\",\n"
       << "      \"kind\": \"Folder\",\n"
       << "      \"visible\": true\n"
       << "    }\n"
       << "  ],\n"
       << "  \"objects\": [\n"
       << "    {\n"
       << "      \"id\": \"world\",\n"
       << "      \"displayName\": \"World\",\n"
       << "      \"kind\": \"Folder\",\n"
       << "      \"visible\": true,\n"
       << "      \"supportsTransform\": false,\n"
       << "      \"transformReadOnly\": true\n"
       << "    }\n"
       << "  ]\n"
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
      .ScriptWorkspace = ResolveProjectScriptWorkspace(Root, *Manifest),
      .Output = ResolveProjectOutputLayout(Root),
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
OpenProjectBySlug(const std::filesystem::path &ProjectsRoot,
                  std::string_view ProjectSlug) {
  if (!IsValidProjectSlug(ProjectSlug)) {
    return std::nullopt;
  }

  const auto Root = ResolveProjectRoot(ProjectsRoot / std::string(ProjectSlug));
  if (!IsPathWithinRoot(ProjectsRoot, Root.RootPath)) {
    return std::nullopt;
  }

  const auto Descriptor = LoadProjectDescriptor(Root.RootPath);
  if (!Descriptor.has_value()) {
    return std::nullopt;
  }
  auto Result = *Descriptor;
  if (Result.Manifest.Slug != ProjectSlug) {
    return std::nullopt;
  }
  if (!EnsureScriptWorkspaceScaffold(Result)) {
    return std::nullopt;
  }
  return Result;
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
      .Version = 2,
      .ProjectId = BuildProjectId(Slug),
      .Name = std::string(ProjectName),
      .Slug = Slug,
      .ScriptAssemblyName = BuildScriptAssemblyName(ProjectName),
      .ScriptRootNamespace = BuildScriptRootNamespace(ProjectName),
  };
  const auto ScriptWorkspace = ResolveProjectScriptWorkspace(Root, Manifest);
  const auto Output = ResolveProjectOutputLayout(Root);
  if (!SaveProjectManifest(Root.ManifestPath, Manifest) ||
      !SaveDefaultSceneFile(Root.SceneFilePath) ||
      !SaveDefaultScriptProject(ScriptWorkspace.ScriptProjectPath,
                                ScriptWorkspace) ||
      !SaveDefaultScriptSolution(ScriptWorkspace.ScriptSolutionPath,
                                 ScriptWorkspace) ||
      !SaveDefaultStarterScript(ScriptWorkspace.StarterScriptPath,
                                ScriptWorkspace) ||
      !EnsureOutputLayoutScaffold(Output)) {
    std::filesystem::remove_all(Root.RootPath, Error);
    if (FailureReason != nullptr) {
      *FailureReason = "Failed to write the initial project scaffold.";
    }
    return std::nullopt;
  }

  return ProjectDescriptor{
      .Manifest = Manifest,
      .Root = Root,
      .ScriptWorkspace = ScriptWorkspace,
      .Output = Output,
  };
}

std::optional<ProjectCookResult>
CookProjectContent(const ProjectDescriptor &Project, std::string *FailureReason) {
  if (!EnsureOutputLayoutScaffold(Project.Output)) {
    if (FailureReason != nullptr) {
      *FailureReason = "Failed to create the project's output directories.";
    }
    return std::nullopt;
  }

  const auto AssetsToCook = CollectCookableAssets(Project.Root.ContentDir);
  for (const auto &RelativeAssetPath : AssetsToCook) {
    const auto Extension = RelativeAssetPath.extension().string();
    if (Extension == ".glb" || Extension == ".gltf" || Extension == ".fbx" ||
        Extension == ".obj") {
      if (!Assets::CookMeshAsset(Project.Root.ContentDir, RelativeAssetPath)
               .has_value()) {
        if (FailureReason != nullptr) {
          *FailureReason = "Failed to cook mesh asset '" +
                           RelativeAssetPath.generic_string() + "'.";
        }
        return std::nullopt;
      }
      continue;
    }

    if (!Assets::CookTextureAsset(Project.Root.ContentDir, RelativeAssetPath)
             .has_value()) {
      if (FailureReason != nullptr) {
        *FailureReason = "Failed to cook texture asset '" +
                         RelativeAssetPath.generic_string() + "'.";
      }
      return std::nullopt;
    }
  }

  const auto Manifest =
      Assets::LoadAssetCookManifest(Project.Output.CookManifestPath)
          .value_or(Assets::AssetCookManifest{});

  return ProjectCookResult{
      .Output = Project.Output,
      .CookedSourceAssetCount = AssetsToCook.size(),
      .ManifestEntryCount = Manifest.Entries.size(),
  };
}

std::optional<ProjectPackageResult>
PackageProjectContent(const ProjectDescriptor &Project,
                      std::string *FailureReason) {
  const auto CookResult = CookProjectContent(Project, FailureReason);
  if (!CookResult.has_value()) {
    return std::nullopt;
  }

  std::error_code Error;
  std::filesystem::remove_all(Project.Output.PackageDir, Error);
  Error.clear();
  std::filesystem::create_directories(Project.Output.PackagedContentDir, Error);
  if (Error) {
    if (FailureReason != nullptr) {
      *FailureReason = "Failed to create the package output directory.";
    }
    return std::nullopt;
  }

  if (!CopyDirectoryTree(Project.Output.CookedDir, Project.Output.PackagedCookedDir)) {
    if (FailureReason != nullptr) {
      *FailureReason = "Failed to copy cooked assets into the package output.";
    }
    return std::nullopt;
  }

  if (std::filesystem::exists(Project.Root.SceneFilePath)) {
    std::filesystem::copy_file(
        Project.Root.SceneFilePath, Project.Output.PackagedSceneFilePath,
        std::filesystem::copy_options::overwrite_existing, Error);
    if (Error) {
      if (FailureReason != nullptr) {
        *FailureReason = "Failed to copy the project scene into the package.";
      }
      return std::nullopt;
    }
  }

  const auto EngineContentDir =
      std::filesystem::path(AXIOM_CONTENT_DIR) / "Engine";
  if (!CopyDirectoryTree(EngineContentDir, Project.Output.PackagedEngineContentDir)) {
    if (FailureReason != nullptr) {
      *FailureReason = "Failed to copy shared engine content into the package.";
    }
    return std::nullopt;
  }

  ProjectPackageResult Result{
      .Cook = *CookResult,
      .PackagedFileCount = CountPackagedFiles(Project.Output.PackageDir),
      .IncludedSceneFile = std::filesystem::exists(Project.Output.PackagedSceneFilePath),
      .IncludedEngineContent =
          std::filesystem::exists(Project.Output.PackagedEngineContentDir),
  };
  if (!SavePackageManifestFile(Project, Result)) {
    if (FailureReason != nullptr) {
      *FailureReason = "Failed to write the package manifest.";
    }
    return std::nullopt;
  }
  Result.PackagedFileCount = CountPackagedFiles(Project.Output.PackageDir);
  return Result;
}

} // namespace Axiom::Project
