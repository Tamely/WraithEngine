#include <Renderer/Mesh.h>
#include <Session/EditorSceneRendererAdapter.h>
#include <Session/EditorSession.h>

#include <gtest/gtest.h>

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
