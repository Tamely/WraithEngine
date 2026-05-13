#include <Project/ProjectSystem.h>
#include <Session/StartupScene.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

namespace {

std::filesystem::path MakeUniqueTempRoot(std::string_view Suffix) {
  return std::filesystem::temp_directory_path() /
         ("wraith-project-tests-" + std::string(Suffix));
}

class ProjectSystemTests : public ::testing::Test {
protected:
  void SetUp() override {
    Root = MakeUniqueTempRoot(::testing::UnitTest::GetInstance()
                                  ->current_test_info()
                                  ->name());
    std::error_code Error;
    std::filesystem::remove_all(Root, Error);
    std::filesystem::create_directories(Root);
  }

  void TearDown() override {
    std::error_code Error;
    std::filesystem::remove_all(Root, Error);
  }

  std::filesystem::path Root;
};

} // namespace

TEST(ProjectSystemStandaloneTests, SlugifyProjectNameNormalizesInput) {
  EXPECT_EQ(Axiom::Project::SlugifyProjectName("My Cool Project"),
            "my-cool-project");
  EXPECT_EQ(Axiom::Project::SlugifyProjectName("  Spaces   & Symbols!! "),
            "spaces-symbols");
  EXPECT_FALSE(Axiom::Project::IsValidProjectSlug("Bad Slug"));
  EXPECT_TRUE(Axiom::Project::IsValidProjectSlug("good-slug-2"));
}

TEST_F(ProjectSystemTests, SaveAndLoadProjectManifestRoundTrips) {
  const auto ProjectRoot = Axiom::Project::ResolveProjectRoot(Root / "alpha");
  const Axiom::Project::ProjectManifest Manifest{
      .Version = 2,
      .ProjectId = "project-alpha",
      .Name = "Alpha",
      .Slug = "alpha",
      .ScriptAssemblyName = "Alpha.Scripts",
      .ScriptRootNamespace = "Alpha.Scripts",
  };

  ASSERT_TRUE(
      Axiom::Project::SaveProjectManifest(ProjectRoot.ManifestPath, Manifest));

  const auto Loaded =
      Axiom::Project::LoadProjectManifest(ProjectRoot.ManifestPath);
  ASSERT_TRUE(Loaded.has_value());
  EXPECT_EQ(Loaded->Version, Manifest.Version);
  EXPECT_EQ(Loaded->ProjectId, Manifest.ProjectId);
  EXPECT_EQ(Loaded->Name, Manifest.Name);
  EXPECT_EQ(Loaded->Slug, Manifest.Slug);
  EXPECT_EQ(Loaded->ScriptAssemblyName, Manifest.ScriptAssemblyName);
  EXPECT_EQ(Loaded->ScriptRootNamespace, Manifest.ScriptRootNamespace);
}

TEST_F(ProjectSystemTests, CreateProjectScaffoldBuildsInitialLayout) {
  std::string FailureReason;
  const auto Created = Axiom::Project::CreateProjectScaffold(
      Root, "Test Project", &FailureReason);

  ASSERT_TRUE(Created.has_value()) << FailureReason;
  EXPECT_EQ(Created->Manifest.Name, "Test Project");
  EXPECT_EQ(Created->Manifest.Slug, "test-project");
  EXPECT_TRUE(std::filesystem::exists(Created->Root.RootPath));
  EXPECT_TRUE(std::filesystem::exists(Created->Root.ManifestPath));
  EXPECT_TRUE(std::filesystem::exists(Created->Root.ContentDir));
  EXPECT_TRUE(std::filesystem::exists(Created->Root.SceneFilePath));
  EXPECT_TRUE(std::filesystem::exists(Created->ScriptWorkspace.ScriptsDir));
  EXPECT_TRUE(std::filesystem::exists(Created->ScriptWorkspace.ScriptProjectPath));
  EXPECT_TRUE(std::filesystem::exists(Created->ScriptWorkspace.ScriptSolutionPath));
  EXPECT_TRUE(std::filesystem::exists(Created->ScriptWorkspace.StarterScriptPath));
  EXPECT_EQ(Created->Manifest.ScriptAssemblyName, "TestProject.Scripts");
  EXPECT_EQ(Created->Manifest.ScriptRootNamespace, "TestProject.Scripts");
  EXPECT_EQ(Created->ScriptWorkspace.AssemblyName, "TestProject.Scripts");
  EXPECT_EQ(Created->ScriptWorkspace.RootNamespace, "TestProject.Scripts");
  EXPECT_EQ(Created->ScriptWorkspace.StarterScriptClassName, "StarterScript");
  EXPECT_EQ(Created->ScriptWorkspace.StarterScriptQualifiedClassName,
            "TestProject.Scripts.StarterScript");

  std::ifstream SceneFile(Created->Root.SceneFilePath);
  ASSERT_TRUE(SceneFile.is_open());
  const std::string SceneText((std::istreambuf_iterator<char>(SceneFile)),
                              std::istreambuf_iterator<char>());
  EXPECT_NE(SceneText.find("\"id\": \"world\""), std::string::npos);
  EXPECT_NE(SceneText.find("\"kind\": \"Folder\""), std::string::npos);

  std::ifstream ScriptProjectFile(Created->ScriptWorkspace.ScriptProjectPath);
  ASSERT_TRUE(ScriptProjectFile.is_open());
  const std::string ScriptProjectText(
      (std::istreambuf_iterator<char>(ScriptProjectFile)),
      std::istreambuf_iterator<char>());
  EXPECT_NE(ScriptProjectText.find("<AssemblyName>TestProject.Scripts</AssemblyName>"),
            std::string::npos);
  EXPECT_NE(ScriptProjectText.find("<RootNamespace>TestProject.Scripts</RootNamespace>"),
            std::string::npos);
  EXPECT_NE(ScriptProjectText.find("WraithEngine.Managed.dll"), std::string::npos);

  std::ifstream SolutionFile(Created->ScriptWorkspace.ScriptSolutionPath);
  ASSERT_TRUE(SolutionFile.is_open());
  const std::string SolutionText((std::istreambuf_iterator<char>(SolutionFile)),
                                 std::istreambuf_iterator<char>());
  EXPECT_NE(SolutionText.find("TestProject.Scripts"), std::string::npos);
  EXPECT_NE(SolutionText.find("Scripts/TestProject.Scripts.csproj"),
            std::string::npos);

  std::ifstream StarterScriptFile(Created->ScriptWorkspace.StarterScriptPath);
  ASSERT_TRUE(StarterScriptFile.is_open());
  const std::string StarterScriptText(
      (std::istreambuf_iterator<char>(StarterScriptFile)),
      std::istreambuf_iterator<char>());
  EXPECT_NE(StarterScriptText.find("using WraithEngine;"), std::string::npos);
  EXPECT_NE(StarterScriptText.find("namespace TestProject.Scripts;"),
            std::string::npos);
  EXPECT_NE(StarterScriptText.find("public class StarterScript : Script"),
            std::string::npos);
}

TEST_F(ProjectSystemTests, DiscoverProjectsReturnsValidProjectsOnly) {
  std::string FailureReason;
  ASSERT_TRUE(Axiom::Project::CreateProjectScaffold(Root, "Bravo", &FailureReason)
                  .has_value())
      << FailureReason;
  ASSERT_TRUE(Axiom::Project::CreateProjectScaffold(Root, "Alpha", &FailureReason)
                  .has_value())
      << FailureReason;

  std::filesystem::create_directories(Root / "not-a-project");
  std::ofstream(Root / "notes.txt") << "ignore me";

  const auto Projects = Axiom::Project::DiscoverProjects(Root);
  ASSERT_EQ(Projects.size(), 2u);
  EXPECT_EQ(Projects[0].Manifest.Name, "Alpha");
  EXPECT_EQ(Projects[1].Manifest.Name, "Bravo");
}

TEST_F(ProjectSystemTests, OpenProjectBySlugLoadsOnlyManagedProjects) {
  std::string FailureReason;
  ASSERT_TRUE(
      Axiom::Project::CreateProjectScaffold(Root, "Gamma", &FailureReason)
          .has_value())
      << FailureReason;

  const auto Opened = Axiom::Project::OpenProjectBySlug(Root, "gamma");
  ASSERT_TRUE(Opened.has_value());
  EXPECT_EQ(Opened->Manifest.Name, "Gamma");
  EXPECT_EQ(Opened->Manifest.Slug, "gamma");
  EXPECT_EQ(Opened->ScriptWorkspace.AssemblyName, "Gamma.Scripts");
  EXPECT_EQ(Opened->ScriptWorkspace.RootNamespace, "Gamma.Scripts");

  EXPECT_FALSE(Axiom::Project::OpenProjectBySlug(Root, "../escape").has_value());
  EXPECT_FALSE(
      Axiom::Project::OpenProjectBySlug(Root, "missing-project").has_value());
}

TEST_F(ProjectSystemTests, LegacyManifestDefaultsScriptWorkspaceFields) {
  const auto ProjectRoot =
      Axiom::Project::ResolveProjectRoot(Root / "legacy-project");
  ASSERT_TRUE(std::filesystem::create_directories(ProjectRoot.RootPath));

  std::ofstream ManifestFile(ProjectRoot.ManifestPath);
  ASSERT_TRUE(ManifestFile.is_open());
  ManifestFile << "{\n"
               << "  \"version\": 1,\n"
               << "  \"projectId\": \"project-legacy\",\n"
               << "  \"name\": \"Legacy Project\",\n"
               << "  \"slug\": \"legacy-project\"\n"
               << "}\n";
  ManifestFile.close();

  const auto Loaded =
      Axiom::Project::LoadProjectDescriptor(ProjectRoot.RootPath);
  ASSERT_TRUE(Loaded.has_value());
  EXPECT_EQ(Loaded->Manifest.ScriptAssemblyName, "LegacyProject.Scripts");
  EXPECT_EQ(Loaded->Manifest.ScriptRootNamespace, "LegacyProject.Scripts");
  EXPECT_EQ(Loaded->ScriptWorkspace.ScriptProjectPath.filename().string(),
            "LegacyProject.Scripts.csproj");

  const auto Opened =
      Axiom::Project::OpenProjectBySlug(Root, "legacy-project");
  ASSERT_TRUE(Opened.has_value());
  EXPECT_TRUE(std::filesystem::exists(Opened->ScriptWorkspace.ScriptProjectPath));
  EXPECT_TRUE(std::filesystem::exists(Opened->ScriptWorkspace.ScriptSolutionPath));
  EXPECT_TRUE(std::filesystem::exists(Opened->ScriptWorkspace.StarterScriptPath));
}

TEST_F(ProjectSystemTests, EmptyProjectSceneLoadsWithoutFallbackContent) {
  std::string FailureReason;
  const auto Created =
      Axiom::Project::CreateProjectScaffold(Root, "Blank Project", &FailureReason);
  ASSERT_TRUE(Created.has_value()) << FailureReason;

  Axiom::EditorSession Session(Axiom::SessionId{77});
  Session.SetContentDir(Created->Root.ContentDir);

  ASSERT_TRUE(Axiom::LoadStartupScene(Session));
  EXPECT_TRUE(Session.GetState().Scene.MeshInstances.empty());
  const Axiom::EditorSceneItem *WorldItem = Session.FindSceneItem("world");
  ASSERT_NE(WorldItem, nullptr);
  EXPECT_EQ(WorldItem->Kind, Axiom::EditorSceneItemKind::Folder);
  EXPECT_TRUE(WorldItem->Children.empty());
  const Axiom::EditorObjectDetails *WorldDetails =
      Session.FindObjectDetails("world");
  ASSERT_NE(WorldDetails, nullptr);
  EXPECT_EQ(WorldDetails->Kind, Axiom::EditorSceneItemKind::Folder);
  EXPECT_FALSE(WorldDetails->SupportsTransform);
}

TEST_F(ProjectSystemTests, IsPathWithinRootRejectsEscapes) {
  const auto ManagedRoot = Root / "managed";
  ASSERT_TRUE(std::filesystem::create_directories(ManagedRoot));
  ASSERT_TRUE(std::filesystem::create_directories(ManagedRoot / "nested"));

  EXPECT_TRUE(Axiom::Project::IsPathWithinRoot(ManagedRoot, ManagedRoot / "nested"));
  EXPECT_FALSE(
      Axiom::Project::IsPathWithinRoot(ManagedRoot, Root.parent_path() / "outside"));
}
