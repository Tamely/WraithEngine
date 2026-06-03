#include "Project/ProjectSystem.h"

#include "Assets/AssetCookManifest.h"
#include "Assets/AssetCooker.h"
#include "Assets/CookedAssetRuntime.h"
#include "Assets/SceneFile.h"
#include "Core/Log.h"

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstdlib>
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

#ifndef AXIOM_PACKAGED_RUNTIME_BINARY_PATH
#define AXIOM_PACKAGED_RUNTIME_BINARY_PATH ""
#endif

namespace Axiom::Project {
namespace {
constexpr std::string_view kDefaultStarterScriptClassName = "StarterScript";
constexpr std::string_view kCsProjectTypeGuid =
    "{FAE04EC0-301F-11D3-BF4B-00C04F79EFBC}";

std::string SerializePrettyJson(const rapidjson::Document &Document) {
  rapidjson::StringBuffer Buffer;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> Writer(Buffer);
  Writer.SetIndent(' ', 2);
  Document.Accept(Writer);
  return std::string(Buffer.GetString(), Buffer.GetSize()) + '\n';
}

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
  rapidjson::Document Document;
  Document.SetObject();
  auto &Allocator = Document.GetAllocator();

  Document.AddMember("version", 1u, Allocator);
  Document.AddMember(
      "projectId",
      rapidjson::Value(Project.Manifest.ProjectId.c_str(),
                       static_cast<rapidjson::SizeType>(
                           Project.Manifest.ProjectId.size()),
                       Allocator)
          .Move(),
      Allocator);
  Document.AddMember(
      "name",
      rapidjson::Value(Project.Manifest.Name.c_str(),
                       static_cast<rapidjson::SizeType>(Project.Manifest.Name.size()),
                       Allocator)
          .Move(),
      Allocator);
  Document.AddMember(
      "slug",
      rapidjson::Value(Project.Manifest.Slug.c_str(),
                       static_cast<rapidjson::SizeType>(Project.Manifest.Slug.size()),
                       Allocator)
          .Move(),
      Allocator);
  Document.AddMember("contentMode", "cooked-only-v1", Allocator);
  Document.AddMember("sceneAsset", "Content/Cooked/scene.wscene", Allocator);
  Document.AddMember("cookedDir", "Content/Cooked", Allocator);
  Document.AddMember("assetCookManifest",
                     "Content/Cooked/AssetCookManifest.json", Allocator);
  Document.AddMember("engineContentDir", "Content/Engine", Allocator);
  Document.AddMember(
      "cookedSourceAssetCount",
      rapidjson::Value()
          .SetUint64(
              static_cast<uint64_t>(PackageResult.Cook.CookedSourceAssetCount)),
      Allocator);
  Document.AddMember(
      "manifestEntryCount",
      rapidjson::Value().SetUint64(
          static_cast<uint64_t>(PackageResult.Cook.ManifestEntryCount)),
      Allocator);

  return WriteTextFile(Project.Output.PackageManifestPath,
                       SerializePrettyJson(Document));
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
      .PackagedSceneAssetPath =
          Root.RootPath / "Package" / "Content" / "Cooked" / "scene.wscene",
      .PackagedEngineContentDir =
          Root.RootPath / "Package" / "Content" / "Engine",
      .PackageManifestPath = Root.RootPath / "Package" / "package.wraith.json",
      .StagedRuntimeBinaryPath =
          Root.RootPath / "Package" /
          std::filesystem::path(AXIOM_PACKAGED_RUNTIME_BINARY_PATH).filename(),
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

  rapidjson::Document Document;
  Document.SetObject();
  auto &Allocator = Document.GetAllocator();

  Document.AddMember("version", Manifest.Version, Allocator);
  Document.AddMember(
      "projectId",
      rapidjson::Value(Manifest.ProjectId.c_str(),
                       static_cast<rapidjson::SizeType>(
                           Manifest.ProjectId.size()),
                       Allocator)
          .Move(),
      Allocator);
  Document.AddMember(
      "name",
      rapidjson::Value(Manifest.Name.c_str(),
                       static_cast<rapidjson::SizeType>(Manifest.Name.size()),
                       Allocator)
          .Move(),
      Allocator);
  Document.AddMember(
      "slug",
      rapidjson::Value(Manifest.Slug.c_str(),
                       static_cast<rapidjson::SizeType>(Manifest.Slug.size()),
                       Allocator)
          .Move(),
      Allocator);
  Document.AddMember(
      "scriptAssemblyName",
      rapidjson::Value(
          Manifest.ScriptAssemblyName.c_str(),
          static_cast<rapidjson::SizeType>(Manifest.ScriptAssemblyName.size()),
          Allocator)
          .Move(),
      Allocator);
  Document.AddMember(
      "scriptRootNamespace",
      rapidjson::Value(
          Manifest.ScriptRootNamespace.c_str(),
          static_cast<rapidjson::SizeType>(Manifest.ScriptRootNamespace.size()),
          Allocator)
          .Move(),
      Allocator);

  return WriteTextFile(ManifestPath, SerializePrettyJson(Document));
}

std::optional<ProjectManifest>
LoadProjectManifest(const std::filesystem::path &ManifestPath) {
  std::string Text;
  if (!ReadFileToString(ManifestPath, Text)) {
    return std::nullopt;
  }

  rapidjson::Document Document;
  Document.ParseInsitu<rapidjson::kParseStopWhenDoneFlag>(Text.data());
  if (Document.HasParseError() || !Document.IsObject()) {
    A_CORE_WARN("ProjectSystem: failed to parse manifest '{}'",
                ManifestPath.string());
    return std::nullopt;
  }

  const auto VersionIt = Document.FindMember("version");
  const auto ProjectIdIt = Document.FindMember("projectId");
  const auto NameIt = Document.FindMember("name");
  const auto SlugIt = Document.FindMember("slug");
  if (VersionIt == Document.MemberEnd() || ProjectIdIt == Document.MemberEnd() ||
      NameIt == Document.MemberEnd() || SlugIt == Document.MemberEnd() ||
      !VersionIt->value.IsUint() || !ProjectIdIt->value.IsString() ||
      !NameIt->value.IsString() || !SlugIt->value.IsString()) {
    return std::nullopt;
  }

  const auto Version = ParseUint32(
      std::to_string(static_cast<std::uint32_t>(VersionIt->value.GetUint())));
  const std::string_view Slug(SlugIt->value.GetString(),
                              SlugIt->value.GetStringLength());
  const std::string_view Name(NameIt->value.GetString(),
                              NameIt->value.GetStringLength());
  if (!Version.has_value() || !IsValidProjectSlug(Slug)) {
    return std::nullopt;
  }

  return ProjectManifest{
      .Version = *Version,
      .ProjectId = std::string(ProjectIdIt->value.GetString(),
                               ProjectIdIt->value.GetStringLength()),
      .Name = std::string(Name),
      .Slug = std::string(Slug),
      .ScriptAssemblyName = [&Document, Name]() {
        const auto ScriptAssemblyIt = Document.FindMember("scriptAssemblyName");
        return ScriptAssemblyIt != Document.MemberEnd() &&
                       ScriptAssemblyIt->value.IsString()
                   ? std::string(ScriptAssemblyIt->value.GetString(),
                                 ScriptAssemblyIt->value.GetStringLength())
                   : BuildScriptAssemblyName(Name);
      }(),
      .ScriptRootNamespace = [&Document, Name]() {
        const auto ScriptNamespaceIt = Document.FindMember("scriptRootNamespace");
        return ScriptNamespaceIt != Document.MemberEnd() &&
                       ScriptNamespaceIt->value.IsString()
                   ? std::string(ScriptNamespaceIt->value.GetString(),
                                 ScriptNamespaceIt->value.GetStringLength())
                   : BuildScriptRootNamespace(Name);
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

  EditorSceneState Scene;
  Scene.Items = {{
      .Id = "world",
      .DisplayName = "World",
      .Kind = EditorSceneItemKind::Folder,
      .Visible = true,
  }};
  Scene.ObjectDetailsById.emplace(
      "world",
      EditorObjectDetails{
          .ObjectId = "world",
          .DisplayName = "World",
          .Kind = EditorSceneItemKind::Folder,
          .Visible = true,
          .SupportsTransform = false,
          .TransformReadOnly = true,
      });
  return Assets::SaveSceneToFile(SceneFilePath, Scene);
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
  if (!std::filesystem::exists(Project.Output.CookManifestPath) &&
      !Assets::SaveAssetCookManifest(Project.Output.CookManifestPath, Manifest)) {
    if (FailureReason != nullptr) {
      *FailureReason = "Failed to write the cooked asset manifest.";
    }
    return std::nullopt;
  }

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

  if (!std::filesystem::exists(Project.Root.SceneFilePath)) {
    if (FailureReason != nullptr) {
      *FailureReason = "Failed to package the project scene because scene.json is missing.";
    }
    return std::nullopt;
  }

  const auto LoadedScene = Assets::LoadSceneFromFile(Project.Root.SceneFilePath);
  if (!LoadedScene.has_value()) {
    if (FailureReason != nullptr) {
      *FailureReason = "Failed to load the project scene before packaging.";
    }
    return std::nullopt;
  }
  if (!Assets::SaveCookedSceneToFile(Project.Output.PackagedSceneAssetPath,
                                     *LoadedScene)) {
    if (FailureReason != nullptr) {
      *FailureReason = "Failed to write the cooked packaged scene asset.";
    }
    return std::nullopt;
  }

  const auto EngineContentDir =
      std::filesystem::path(AXIOM_CONTENT_DIR) / "Engine";
  if (!CopyDirectoryTree(EngineContentDir, Project.Output.PackagedEngineContentDir)) {
    if (FailureReason != nullptr) {
      *FailureReason = "Failed to copy shared engine content into the package.";
    }
    return std::nullopt;
  }

  const std::filesystem::path RuntimeBinaryPath =
      std::filesystem::path(AXIOM_PACKAGED_RUNTIME_BINARY_PATH);
  if (RuntimeBinaryPath.empty() || !std::filesystem::exists(RuntimeBinaryPath)) {
    if (FailureReason != nullptr) {
      *FailureReason =
          "Failed to stage AxiomPackagedRuntime because the built runtime binary was not found.";
    }
    return std::nullopt;
  }

  Error.clear();
  std::filesystem::copy_file(
      RuntimeBinaryPath, Project.Output.StagedRuntimeBinaryPath,
      std::filesystem::copy_options::overwrite_existing, Error);
  if (Error) {
    if (FailureReason != nullptr) {
      *FailureReason =
          "Failed to copy AxiomPackagedRuntime into the package output.";
    }
    return std::nullopt;
  }
#ifndef _WIN32
  std::filesystem::permissions(
      Project.Output.StagedRuntimeBinaryPath,
      std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec |
          std::filesystem::perms::others_exec,
      std::filesystem::perm_options::add, Error);
  Error.clear();
#endif

  ProjectPackageResult Result{
      .Cook = *CookResult,
      .PackagedFileCount = CountPackagedFiles(Project.Output.PackageDir),
      .IncludedSceneAsset =
          std::filesystem::exists(Project.Output.PackagedSceneAssetPath),
      .IncludedEngineContent =
          std::filesystem::exists(Project.Output.PackagedEngineContentDir),
      .IncludedRuntimeBinary =
          std::filesystem::exists(Project.Output.StagedRuntimeBinaryPath),
      .SceneAssetPath = Project.Output.PackagedSceneAssetPath,
      .RuntimeBinaryPath = Project.Output.StagedRuntimeBinaryPath,
  };
  if (!SavePackageManifestFile(Project, Result)) {
    if (FailureReason != nullptr) {
      *FailureReason = "Failed to write the package manifest.";
    }
    return std::nullopt;
  }
  std::string ValidationFailureReason;
  const auto PackagedDescriptor =
      Assets::ResolvePackagedContentDescriptor(Project.Output.PackagedContentDir,
                                               &ValidationFailureReason);
  if (!PackagedDescriptor.has_value()) {
    if (FailureReason != nullptr) {
      *FailureReason = "Packaged content validation failed: " +
                       ValidationFailureReason;
    }
    return std::nullopt;
  }
  if (!Assets::ValidatePackagedContentDescriptor(*PackagedDescriptor,
                                                 &ValidationFailureReason)) {
    if (FailureReason != nullptr) {
      *FailureReason = "Packaged content validation failed: " +
                       ValidationFailureReason;
    }
    return std::nullopt;
  }
  Result.PackagedFileCount = CountPackagedFiles(Project.Output.PackageDir);
  return Result;
}

} // namespace Axiom::Project
