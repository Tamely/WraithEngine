#include <Project/ProjectSystem.h>

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
      .Version = 1,
      .ProjectId = "project-alpha",
      .Name = "Alpha",
      .Slug = "alpha",
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

  std::ifstream SceneFile(Created->Root.SceneFilePath);
  ASSERT_TRUE(SceneFile.is_open());
  const std::string SceneText((std::istreambuf_iterator<char>(SceneFile)),
                              std::istreambuf_iterator<char>());
  EXPECT_NE(SceneText.find("\"nodes\": []"), std::string::npos);
  EXPECT_NE(SceneText.find("\"objects\": []"), std::string::npos);
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

  EXPECT_FALSE(Axiom::Project::OpenProjectBySlug(Root, "../escape").has_value());
  EXPECT_FALSE(
      Axiom::Project::OpenProjectBySlug(Root, "missing-project").has_value());
}

TEST_F(ProjectSystemTests, IsPathWithinRootRejectsEscapes) {
  const auto ManagedRoot = Root / "managed";
  ASSERT_TRUE(std::filesystem::create_directories(ManagedRoot));
  ASSERT_TRUE(std::filesystem::create_directories(ManagedRoot / "nested"));

  EXPECT_TRUE(Axiom::Project::IsPathWithinRoot(ManagedRoot, ManagedRoot / "nested"));
  EXPECT_FALSE(
      Axiom::Project::IsPathWithinRoot(ManagedRoot, Root.parent_path() / "outside"));
}
