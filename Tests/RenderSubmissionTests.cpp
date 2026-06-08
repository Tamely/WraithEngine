#include <Core/Log.h>
#include <Core/VulkanLoader.h>
#include <Renderer/Mesh.h>
#include <Renderer/Camera.h>
#include <Jobs/JobSystem.h>
#include <Renderer/OffscreenRenderSurface.h>
#include <Renderer/RenderCommand.h>
#include <Renderer/Renderer.h>
#include <Session/EditorSceneRendererAdapter.h>
#include <Session/EditorSession.h>

#include <gtest/gtest.h>

#include <glm/ext/matrix_transform.hpp>

#include <mutex>
#include <memory>
#include <utility>

namespace {
class FakeMesh final : public Axiom::Mesh {
public:
  explicit FakeMesh(Axiom::MeshHandle Handle) { AssignHandle(Handle); }
};

struct CountingMeshFactory {
  Axiom::RenderMeshResource operator()(const Axiom::MeshData &) {
    ++CreateCount;
    Axiom::MeshHandle Handle{NextHandleValue++};
    return {.Handle = Handle, .Mesh = std::make_shared<FakeMesh>(Handle)};
  }

  int CreateCount{0};
  uint64_t NextHandleValue{1};
};

template <typename T>
concept HasLegacyTypedMeshMember = requires(T Submission) {
  Submission.TypedMesh;
};

template <typename T>
concept HasLegacyMeshRefMember = requires(T Submission) {
  Submission.Mesh;
};

template <typename T>
concept HasOpaqueMeshHandleMember = requires(T Submission) {
  Submission.MeshHandle;
};

static_assert(!HasLegacyTypedMeshMember<Axiom::RenderMeshSubmission>);
static_assert(!HasLegacyMeshRefMember<Axiom::RenderMeshSubmission>);
static_assert(HasOpaqueMeshHandleMember<Axiom::RenderMeshSubmission>);

Axiom::EditorSceneMeshInstance MakeMeshInstance(std::string ObjectId,
                                                std::string AssetRelativePath) {
  return {
      .ObjectId = std::move(ObjectId),
      .Mesh = Axiom::MeshData{
          .Vertices = {{{0.0f, 0.0f, 0.0f},
                        {0.0f, 0.0f, 1.0f},
                        {0.0f, 0.0f}},
                       {{1.0f, 0.0f, 0.0f},
                        {0.0f, 0.0f, 1.0f},
                        {1.0f, 0.0f}},
                       {{0.0f, 1.0f, 0.0f},
                        {0.0f, 0.0f, 1.0f},
                        {0.0f, 1.0f}}},
          .Indices = {0, 1, 2},
          .BoundsMin = {0.0f, 0.0f, 0.0f},
          .BoundsMax = {1.0f, 1.0f, 0.0f},
      },
      .Material = std::make_shared<Axiom::MaterialInstance>(),
      .RenderPath = Axiom::MeshRenderPath::Graphics,
      .Transform = glm::mat4(1.0f),
      .AssetRelativePath = std::move(AssetRelativePath),
  };
}

Axiom::Camera MakeRenderCamera() {
  static std::once_flag Flag;
  std::call_once(Flag, []() { Axiom::Log::Init(); });
  Axiom::Camera Camera;
  Camera.LookAt({0.0f, 0.0f, 6.0f}, {0.0f, 0.0f, 0.0f});
  Camera.SetPerspective(55.0f, 1280.0f / 720.0f, 0.1f, 100.0f);
  return Camera;
}

void EnsureLoggingInitialized() {
  static std::once_flag Flag;
  std::call_once(Flag, []() { Axiom::Log::Init(); });
}

Axiom::MeshData MakeTriangleMesh() {
  return {
      .Vertices = {{{-0.25f, -0.25f, 0.0f},
                    {0.0f, 0.0f, 1.0f},
                    {0.0f, 0.0f}},
                   {{0.25f, -0.25f, 0.0f},
                    {0.0f, 0.0f, 1.0f},
                    {1.0f, 0.0f}},
                   {{0.0f, 0.25f, 0.0f},
                    {0.0f, 0.0f, 1.0f},
                    {0.5f, 1.0f}}},
      .Indices = {0, 1, 2},
      .BoundsMin = {-0.25f, -0.25f, 0.0f},
      .BoundsMax = {0.25f, 0.25f, 0.0f},
  };
}

class ScopedJobSystem {
public:
  ScopedJobSystem() { Axiom::Jobs::Startup(); }
  ~ScopedJobSystem() { Axiom::Jobs::Shutdown(); }
};
} // namespace

TEST(RenderSubmissionTests, EditorSceneRendererAdapterReusesCachedMeshUntilAssetChanges) {
  auto Factory = std::make_shared<CountingMeshFactory>();
  Axiom::EditorSceneRendererAdapter Adapter(
      [Factory](const Axiom::MeshData &Mesh) { return (*Factory)(Mesh); });
  Axiom::EditorSession Session(Axiom::SessionId{1});

  Session.SetSceneMeshInstances({MakeMeshInstance("crate", "Meshes/crate-a.glb")});
  const std::vector<Axiom::RenderMeshSubmission> First =
      Adapter.BuildRenderSubmissions(Session);
  const std::vector<Axiom::RenderMeshSubmission> Second =
      Adapter.BuildRenderSubmissions(Session);

  ASSERT_EQ(First.size(), 1u);
  ASSERT_EQ(Second.size(), 1u);
  EXPECT_EQ(Factory->CreateCount, 1);
  EXPECT_EQ(First[0].MeshHandle, Second[0].MeshHandle);
  EXPECT_EQ(First[0].DebugDataId, Second[0].DebugDataId);
  EXPECT_EQ(Axiom::GetRenderMeshSubmissionDebugName(First[0].DebugDataId), "crate");

  Session.SetSceneMeshInstances({MakeMeshInstance("crate", "Meshes/crate-b.glb")});
  const std::vector<Axiom::RenderMeshSubmission> Swapped =
      Adapter.BuildRenderSubmissions(Session);

  ASSERT_EQ(Swapped.size(), 1u);
  EXPECT_EQ(Factory->CreateCount, 2);
  EXPECT_NE(Swapped[0].MeshHandle, First[0].MeshHandle);
  EXPECT_EQ(Swapped[0].DebugDataId, First[0].DebugDataId);
}

TEST(RenderSubmissionTests, EditorSceneRendererAdapterDropsDeletedObjectsFromCache) {
  auto Factory = std::make_shared<CountingMeshFactory>();
  Axiom::EditorSceneRendererAdapter Adapter(
      [Factory](const Axiom::MeshData &Mesh) { return (*Factory)(Mesh); });
  Axiom::EditorSession Session(Axiom::SessionId{2});

  Session.SetSceneMeshInstances({MakeMeshInstance("crate", "Meshes/crate.glb")});
  const std::vector<Axiom::RenderMeshSubmission> First =
      Adapter.BuildRenderSubmissions(Session);
  ASSERT_EQ(First.size(), 1u);
  EXPECT_EQ(Factory->CreateCount, 1);

  Session.SetSceneMeshInstances({});
  const std::vector<Axiom::RenderMeshSubmission> Empty =
      Adapter.BuildRenderSubmissions(Session);
  EXPECT_TRUE(Empty.empty());
  EXPECT_EQ(Factory->CreateCount, 1);

  Session.SetSceneMeshInstances({MakeMeshInstance("crate", "Meshes/crate.glb")});
  const std::vector<Axiom::RenderMeshSubmission> Recreated =
      Adapter.BuildRenderSubmissions(Session);

  ASSERT_EQ(Recreated.size(), 1u);
  EXPECT_EQ(Factory->CreateCount, 2);
  EXPECT_NE(Recreated[0].MeshHandle, First[0].MeshHandle);
}

TEST(RenderSubmissionTests,
     VulkanRendererRendersAllTenThousandSubmittedMeshesOffscreen) {
  constexpr uint32_t Width = 1280;
  constexpr uint32_t Height = 720;
  constexpr size_t MeshCount = 10000;

  EnsureLoggingInitialized();
  if (!Axiom::CanInitializeHeadlessVulkan()) {
    GTEST_SKIP() << "Headless Vulkan is unavailable on this host";
  }

  ScopedJobSystem Jobs;
  auto Surface = std::make_shared<Axiom::OffscreenRenderSurface>(Width, Height);
  Axiom::Renderer Renderer;
  Renderer.Init({
      .TargetSurface = Surface,
      .Width = Width,
      .Height = Height,
      .EnableParallelCull = true,
      .VerifyParallelCull = true,
  });
  Renderer.SetViewMode(Axiom::RendererViewMode::Wireframe);

  Axiom::RenderMeshResource MeshResource =
      Renderer.CreateMeshResource(MakeTriangleMesh());
  ASSERT_TRUE(MeshResource.IsValid());
  const Axiom::MeshHandle MeshHandle = MeshResource.Handle;

  const Axiom::Camera Camera = MakeRenderCamera();
  const auto SubmitScene = [&]() {
    Renderer.BeginFrame();
    Axiom::RenderCommand::SetCamera(Camera);
    for (size_t Index = 0; Index < MeshCount; ++Index) {
      const float X = static_cast<float>(Index % 100u) * 0.05f - 2.5f;
      const float Y = static_cast<float>(Index / 100u) * 0.05f - 1.25f;
      Axiom::RenderCommand::Submit({
          .MeshHandle = MeshHandle,
          .DebugDataId = Axiom::RegisterRenderMeshSubmissionDebugData(
              {.Name = "submission-" + std::to_string(Index)}),
          .Transform =
              glm::translate(glm::mat4(1.0f), glm::vec3(X, Y, 0.0f)),
      });
    }
    Renderer.Render();
    Renderer.EndFrame();
  };

  SubmitScene();
  const Axiom::RendererFrameStats Stats = Renderer.GetFrameStats();
  EXPECT_EQ(Stats.SubmittedMeshCount, MeshCount);
  EXPECT_EQ(Stats.MeshSubmissionCount, MeshCount);
  EXPECT_EQ(Stats.FrustumCulledMeshCount, 0u);
  EXPECT_EQ(Stats.OcclusionCulledMeshCount, 0u);
  EXPECT_EQ(Stats.TriangleCount, MeshCount);

  SubmitScene();
  SubmitScene();
  std::optional<Axiom::CapturedFrame> Captured = Renderer.ConsumeCapturedFrame();
  ASSERT_TRUE(Captured.has_value());
  EXPECT_EQ(Captured->Width, Width);
  EXPECT_EQ(Captured->Height, Height);
  EXPECT_FALSE(Captured->Pixels.empty());

  MeshResource.Mesh.reset();
  Renderer.Shutdown();
}
