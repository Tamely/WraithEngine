#include <gtest/gtest.h>

#include <Renderer/Camera.h>
#include <Session/MeshPicking.h>

#include <glm/ext/matrix_transform.hpp>

namespace {

Axiom::Camera MakeCamera(const glm::vec3 &Position, const glm::vec3 &Target) {
  Axiom::Camera Camera;
  Camera.LookAt(Position, Target);
  Camera.SetPerspective(55.0f, 1280.0f / 720.0f, 0.1f, 100.0f);
  return Camera;
}

} // namespace

TEST(MeshPickingTests, HitTestMeshesDetailedReturnsWorldHitPosition) {
  const Axiom::Camera Camera = MakeCamera({0.0f, 1.0f, 5.0f}, {0.0f, 1.0f, 0.0f});
  std::vector<Axiom::EditorSceneMeshInstance> Instances{{
      .ObjectId = "crate",
      .Mesh = Axiom::MeshData{
          .BoundsMin = {-0.5f, -0.5f, -0.5f},
          .BoundsMax = {0.5f, 0.5f, 0.5f},
      },
      .Transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.5f, 0.0f)),
  }};

  const auto Hit =
      Axiom::HitTestMeshesDetailed(Camera, 1280, 720, {640.0f, 360.0f}, Instances);

  ASSERT_TRUE(Hit.has_value());
  EXPECT_EQ(Hit->ObjectId, "crate");
  EXPECT_NEAR(Hit->WorldPosition.x, 0.0f, 1e-3f);
  EXPECT_NEAR(Hit->WorldPosition.y, 1.0f, 1e-3f);
  EXPECT_NEAR(Hit->WorldPosition.z, 0.5f, 1e-3f);
}

TEST(MeshPickingTests, HitTestLightBillboardsDetailedReturnsLightHit) {
  const Axiom::Camera Camera = MakeCamera({0.0f, 1.0f, 5.0f}, {0.0f, 1.0f, 0.0f});
  const std::vector<Axiom::LightBillboardOverlay> Billboards{{
      .ObjectId = "key-light",
      .WorldPosition = {0.0f, 1.0f, 0.0f},
      .PixelSize = 48.0f,
  }};

  const auto Hit = Axiom::HitTestLightBillboardsDetailed(
      Camera, 1280, 720, {640.0f, 360.0f}, Billboards);

  ASSERT_TRUE(Hit.has_value());
  EXPECT_EQ(Hit->ObjectId, "key-light");
  EXPECT_NEAR(Hit->WorldPosition.x, 0.0f, 1e-3f);
  EXPECT_NEAR(Hit->WorldPosition.y, 1.0f, 1e-3f);
  EXPECT_NEAR(Hit->WorldPosition.z, 0.0f, 1e-3f);
  EXPECT_NEAR(Hit->Distance, 5.0f, 1e-3f);
}

TEST(MeshPickingTests, HitTestLightBillboardsDetailedMissesOutsideQuad) {
  const Axiom::Camera Camera = MakeCamera({0.0f, 1.0f, 5.0f}, {0.0f, 1.0f, 0.0f});
  const std::vector<Axiom::LightBillboardOverlay> Billboards{{
      .ObjectId = "key-light",
      .WorldPosition = {0.0f, 1.0f, 0.0f},
      .PixelSize = 48.0f,
  }};

  const auto Hit =
      Axiom::HitTestLightBillboardsDetailed(Camera, 1280, 720, {0.0f, 0.0f}, Billboards);

  EXPECT_FALSE(Hit.has_value());
}

TEST(MeshPickingTests, ResolveViewportSelectionHitChoosesNearestObjectHit) {
  const Axiom::Camera Camera = MakeCamera({0.0f, 1.0f, 5.0f}, {0.0f, 1.0f, 0.0f});
  const std::vector<Axiom::EditorSceneMeshInstance> Instances{{
      .ObjectId = "crate",
      .Mesh = Axiom::MeshData{
          .BoundsMin = {-0.5f, -0.5f, -0.5f},
          .BoundsMax = {0.5f, 0.5f, 0.5f},
      },
      .Transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 2.0f)),
  }};
  const std::vector<Axiom::LightBillboardOverlay> Billboards{{
      .ObjectId = "key-light",
      .WorldPosition = {0.0f, 1.0f, 0.0f},
      .PixelSize = 48.0f,
  }};

  const auto Hit = Axiom::ResolveViewportSelectionHit(
      Camera, 1280, 720, {640.0f, 360.0f}, Instances, Billboards);

  ASSERT_TRUE(Hit.has_value());
  EXPECT_EQ(Hit->ObjectId, "crate");
  EXPECT_LT(Hit->Distance, 5.0f);
}

TEST(MeshPickingTests, ResolveViewportSelectionHitUsesBillboardWhenMeshMisses) {
  const Axiom::Camera Camera = MakeCamera({0.0f, 1.0f, 5.0f}, {0.0f, 1.0f, 0.0f});
  const std::vector<Axiom::LightBillboardOverlay> Billboards{{
      .ObjectId = "key-light",
      .WorldPosition = {0.0f, 1.0f, 0.0f},
      .PixelSize = 48.0f,
  }};

  const auto Hit = Axiom::ResolveViewportSelectionHit(
      Camera, 1280, 720, {640.0f, 360.0f}, {}, Billboards);

  ASSERT_TRUE(Hit.has_value());
  EXPECT_EQ(Hit->ObjectId, "key-light");
}

TEST(MeshPickingTests, ResolveViewportDropPositionFallsBackToGroundPlane) {
  const Axiom::Camera Camera = MakeCamera({0.0f, 1.0f, 5.0f}, {0.0f, 0.0f, 0.0f});

  const glm::vec3 Position = Axiom::ResolveViewportDropPosition(
      Camera, 1280, 720, {640.0f, 360.0f}, {});

  EXPECT_NEAR(Position.x, 0.0f, 1e-3f);
  EXPECT_NEAR(Position.y, 0.0f, 1e-3f);
  EXPECT_NEAR(Position.z, 0.0f, 1e-2f);
}

TEST(MeshPickingTests, ResolveViewportDropPositionFallsBackInFrontOfCamera) {
  const Axiom::Camera Camera = MakeCamera({0.0f, 1.0f, 5.0f}, {0.0f, 1.0f, 0.0f});

  const glm::vec3 Position = Axiom::ResolveViewportDropPosition(
      Camera, 1280, 720, {640.0f, 360.0f}, {});

  EXPECT_NEAR(Position.x, 0.0f, 1e-3f);
  EXPECT_NEAR(Position.y, 1.0f, 1e-3f);
  EXPECT_NEAR(Position.z, 2.0f, 1e-3f);
}
