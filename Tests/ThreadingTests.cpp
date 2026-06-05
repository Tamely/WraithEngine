#include <gtest/gtest.h>

#include <Core/Log.h>
#include <Core/VulkanLoader.h>
#include <Jobs/JobSystem.h>
#include <Renderer/Camera.h>
#include <Renderer/RenderCommand.h>
#include <Renderer/Renderer.h>
#include <Renderer/RenderSurface.h>
#if AXIOM_WITH_PHYSICS
#include <Session/EditorPhysicsController.h>
#endif
#include <Session/EditorSession.h>

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

namespace {
void EnsureLoggingInitialized() {
  static bool Initialized = false;
  if (!Initialized) {
    Axiom::Log::Init();
    Initialized = true;
  }
}

Axiom::MeshData MakeTriangleMesh() {
  Axiom::MeshData Mesh;
  Mesh.Vertices = {
      {{-0.5f, -0.5f, 0.0f}},
      {{0.5f, -0.5f, 0.0f}},
      {{0.0f, 0.5f, 0.0f}},
  };
  Mesh.Indices = {0, 1, 2};
  Mesh.BoundsMin = {-0.5f, -0.5f, 0.0f};
  Mesh.BoundsMax = {0.5f, 0.5f, 0.0f};
  return Mesh;
}

Axiom::Camera MakeRenderCamera() {
  Axiom::Camera Camera;
  Camera.LookAt({0.0f, 0.0f, 6.0f}, {0.0f, 0.0f, 0.0f});
  Camera.SetPerspective(55.0f, 1280.0f / 720.0f, 0.1f, 100.0f);
  return Camera;
}

Axiom::CommandContext MakeContext(uint64_t FrameIndex = 1,
                                  uint64_t UserId = 1) {
  return {
      .Session = Axiom::SessionId{1},
      .User = Axiom::SessionUserId{UserId},
      .FrameIndex = FrameIndex,
      .DeltaTimeSeconds = 1.0f / 60.0f,
  };
}
} // namespace

TEST(ThreadingTests, JobsRunWithDependenciesAndParallelFor) {
  Axiom::Jobs::Startup();

  std::atomic<int> Value{0};
  Axiom::Jobs::JobHandle First = Axiom::Jobs::ScheduleJob([&Value]() {
    Value.fetch_add(1, std::memory_order_relaxed);
  });

  std::array<Axiom::Jobs::JobHandle, 1> Dependencies = {First};
  Axiom::Jobs::JobHandle Second = Axiom::Jobs::ScheduleJobAfter(
      [&Value]() { Value.fetch_add(2, std::memory_order_relaxed); },
      std::span<Axiom::Jobs::JobHandle>(Dependencies));
  Axiom::Jobs::Wait(Second);

  std::atomic<size_t> ParallelSum{0};
  Axiom::Jobs::ParallelFor(256, [&ParallelSum](size_t Index) {
    ParallelSum.fetch_add(Index + 1, std::memory_order_relaxed);
  });

  EXPECT_EQ(Value.load(std::memory_order_relaxed), 3);
  EXPECT_EQ(ParallelSum.load(std::memory_order_relaxed), (256u * 257u) / 2u);

  Axiom::Jobs::Shutdown();
}

TEST(ThreadingTests, ThreadedRendererRunsHeadlessForThousandFramesWithoutDeadlock) {
#if AXIOM_THREADED_RENDER == 0
  GTEST_SKIP() << "Threaded renderer is disabled in this build";
#else
  constexpr uint32_t Width = 64;
  constexpr uint32_t Height = 64;
#if AXIOM_THREAD_SANITIZER
  constexpr size_t FrameCount = 1000;
#else
  constexpr size_t FrameCount = 128;
#endif

  EnsureLoggingInitialized();
  if (!Axiom::CanInitializeHeadlessVulkan()) {
    GTEST_SKIP() << "Headless Vulkan is unavailable on this host";
  }

  auto Surface = std::make_shared<Axiom::OffscreenRenderSurface>(Width, Height);
  Axiom::Renderer Renderer;
  Renderer.Init({
      .TargetSurface = Surface,
      .Width = Width,
      .Height = Height,
      .EnableThreadedRendering = true,
  });

  Axiom::RenderMeshResource MeshResource =
      Renderer.CreateMeshResource(MakeTriangleMesh());
  ASSERT_TRUE(MeshResource.IsValid());

  const Axiom::Camera Camera = MakeRenderCamera();
  for (size_t FrameIndex = 0; FrameIndex < FrameCount; ++FrameIndex) {
    Renderer.SetCpuFrameTime(16.0f);
    Renderer.BeginFrame();
    Axiom::RenderCommand::SetCamera(Camera);
    Axiom::RenderCommand::Submit({
        .MeshHandle = MeshResource.Handle,
        .DebugDataId = Axiom::RegisterRenderMeshSubmissionDebugData(
            {.Name = "threaded-frame-" + std::to_string(FrameIndex)}),
    });
    Renderer.Render();
    Renderer.EndFrame();
  }

  Renderer.WaitForIdle();
  MeshResource.Mesh.reset();
  Renderer.Shutdown();
#endif
}

TEST(ThreadingTests, ThreadedRendererOverlapsGameThreadRecordingWithRenderThreadWork) {
#if AXIOM_THREADED_RENDER == 0
  GTEST_SKIP() << "Threaded renderer is disabled in this build";
#else
  constexpr uint32_t Width = 64;
  constexpr uint32_t Height = 64;

  EnsureLoggingInitialized();
  if (!Axiom::CanInitializeHeadlessVulkan()) {
    GTEST_SKIP() << "Headless Vulkan is unavailable on this host";
  }

  std::mutex Mutex;
  std::condition_variable RenderStartedCv;
  bool FirstRenderStarted = false;
  std::thread::id RenderThreadId;
  const std::thread::id GameThreadId = std::this_thread::get_id();

  auto Surface = std::make_shared<Axiom::OffscreenRenderSurface>(Width, Height);
  Axiom::Renderer Renderer;
  Renderer.Init({
      .TargetSurface = Surface,
      .Width = Width,
      .Height = Height,
      .EnableThreadedRendering = true,
      .ThreadedRenderSceneStartCallback =
          [&](uint64_t FrameNumber) {
            {
              std::scoped_lock Lock(Mutex);
              RenderThreadId = std::this_thread::get_id();
            }
            if (FrameNumber == 1) {
              {
                std::scoped_lock Lock(Mutex);
                FirstRenderStarted = true;
              }
              RenderStartedCv.notify_all();
              std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
          },
  });

  Axiom::RenderMeshResource MeshResource =
      Renderer.CreateMeshResource(MakeTriangleMesh());
  ASSERT_TRUE(MeshResource.IsValid());
  const Axiom::Camera Camera = MakeRenderCamera();

  const auto SubmitFrame = [&](size_t FrameIndex) {
    Renderer.SetCpuFrameTime(16.0f);
    Renderer.BeginFrame();
    Axiom::RenderCommand::SetCamera(Camera);
    Axiom::RenderCommand::Submit({
        .MeshHandle = MeshResource.Handle,
        .DebugDataId = Axiom::RegisterRenderMeshSubmissionDebugData(
            {.Name = "overlap-frame-" + std::to_string(FrameIndex)}),
    });
    Renderer.Render();
    Renderer.EndFrame();
  };

  SubmitFrame(1);
  {
    std::unique_lock Lock(Mutex);
    RenderStartedCv.wait(Lock, [&]() { return FirstRenderStarted; });
  }

  const auto OverlapWindowStart = std::chrono::steady_clock::now();
  SubmitFrame(2);
  SubmitFrame(3);
  const auto OverlapWindowMs =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - OverlapWindowStart);

  EXPECT_LT(OverlapWindowMs.count(), 40);
  EXPECT_NE(RenderThreadId, std::thread::id{});
  EXPECT_NE(RenderThreadId, GameThreadId);

  Renderer.WaitForIdle();
  MeshResource.Mesh.reset();
  Renderer.Shutdown();
#endif
}

TEST(ThreadingTests,
     ThreadedRendererAllowsPhysicsSimulationOverlapWithRenderThreadWork) {
#if AXIOM_THREADED_RENDER == 0
  GTEST_SKIP() << "Threaded renderer is disabled in this build";
#elif !AXIOM_WITH_PHYSICS
  GTEST_SKIP() << "Physics backend disabled for this build.";
#else
  constexpr uint32_t Width = 64;
  constexpr uint32_t Height = 64;

  EnsureLoggingInitialized();
  if (!Axiom::CanInitializeHeadlessVulkan()) {
    GTEST_SKIP() << "Headless Vulkan is unavailable on this host";
  }

  std::mutex Mutex;
  std::condition_variable RenderStartedCv;
  bool FirstRenderStarted = false;
  bool FirstRenderCompleted = false;

  auto Surface = std::make_shared<Axiom::OffscreenRenderSurface>(Width, Height);
  Axiom::Renderer Renderer;
  Renderer.Init({
      .TargetSurface = Surface,
      .Width = Width,
      .Height = Height,
      .EnableThreadedRendering = true,
      .ThreadedRenderSceneStartCallback =
          [&](uint64_t FrameNumber) {
            if (FrameNumber != 1) {
              return;
            }
            {
              std::scoped_lock Lock(Mutex);
              FirstRenderStarted = true;
            }
            RenderStartedCv.notify_all();
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
          },
      .ThreadedRenderSceneCompleteCallback =
          [&](uint64_t FrameNumber) {
            if (FrameNumber != 1) {
              return;
            }
            std::scoped_lock Lock(Mutex);
            FirstRenderCompleted = true;
          },
  });

  Axiom::RenderMeshResource MeshResource =
      Renderer.CreateMeshResource(MakeTriangleMesh());
  ASSERT_TRUE(MeshResource.IsValid());
  const Axiom::Camera Camera = MakeRenderCamera();

  Axiom::EditorSession Session(Axiom::SessionId{1});
  Axiom::AttachEditorPhysicsController(Session);
  Session.SetSceneItems({{
      .Id = "world",
      .DisplayName = "World",
      .Kind = Axiom::EditorSceneItemKind::Folder,
      .Visible = true,
      .Children = {{
                       .Id = "floor",
                       .DisplayName = "Floor",
                       .Kind = Axiom::EditorSceneItemKind::Mesh,
                       .Visible = true,
                   },
                   {
                       .Id = "ball",
                       .DisplayName = "Ball",
                       .Kind = Axiom::EditorSceneItemKind::Actor,
                       .Visible = true,
                   }},
  }});
  Session.SetObjectDetails({
      {
          .ObjectId = "world",
          .DisplayName = "World",
          .Kind = Axiom::EditorSceneItemKind::Folder,
          .Visible = true,
          .SupportsTransform = false,
          .TransformReadOnly = true,
      },
      {
          .ObjectId = "floor",
          .DisplayName = "Floor",
          .Kind = Axiom::EditorSceneItemKind::Mesh,
          .Visible = true,
          .SupportsTransform = true,
          .TransformReadOnly = false,
          .Transform = Axiom::EditorTransformDetails{
              .Location = glm::vec3(0.0f, -1.0f, 0.0f),
              .RotationDegrees = glm::vec3(0.0f),
              .Scale = glm::vec3(1.0f),
          },
          .Physics = Axiom::EditorPhysicsProperties{
              .BodyType = Axiom::EditorPhysicsBodyType::Static,
              .ColliderType = Axiom::EditorPhysicsColliderType::Box,
              .BoxHalfExtents = glm::vec3(8.0f, 0.5f, 8.0f),
          },
      },
      {
          .ObjectId = "ball",
          .DisplayName = "Ball",
          .Kind = Axiom::EditorSceneItemKind::Actor,
          .Visible = true,
          .SupportsTransform = true,
          .TransformReadOnly = false,
          .Transform = Axiom::EditorTransformDetails{
              .Location = glm::vec3(0.0f, 5.0f, 0.0f),
              .RotationDegrees = glm::vec3(0.0f),
              .Scale = glm::vec3(1.0f),
          },
          .Physics = Axiom::EditorPhysicsProperties{
              .BodyType = Axiom::EditorPhysicsBodyType::Dynamic,
              .ColliderType = Axiom::EditorPhysicsColliderType::Sphere,
              .SphereRadius = 0.5f,
              .Mass = 1.0f,
          },
      },
  });
  Session.Submit(MakeContext(1, 1), {.Payload = Axiom::PlaySessionCommand{}});
  Session.Tick(1.0f / 60.0f);

  Renderer.SetCpuFrameTime(16.0f);
  Renderer.BeginFrame();
  Axiom::RenderCommand::SetCamera(Camera);
  Axiom::RenderCommand::Submit({
      .MeshHandle = MeshResource.Handle,
      .DebugDataId = Axiom::RegisterRenderMeshSubmissionDebugData(
          {.Name = "physics-overlap-frame"}),
  });
  Renderer.Render();
  Renderer.EndFrame();

  {
    std::unique_lock Lock(Mutex);
    RenderStartedCv.wait(Lock, [&]() { return FirstRenderStarted; });
  }

  const auto PhysicsStart = std::chrono::steady_clock::now();
  for (int Step = 0; Step < 30; ++Step) {
    Session.Tick(1.0f / 60.0f);
  }
  const auto PhysicsElapsed =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - PhysicsStart);

  const Axiom::EditorObjectDetails *Ball = Session.FindObjectDetails("ball");
  ASSERT_NE(Ball, nullptr);
  ASSERT_TRUE(Ball->WorldTransform.has_value() || Ball->Transform.has_value());
  const Axiom::EditorTransformDetails &Transform =
      Ball->WorldTransform.has_value() ? *Ball->WorldTransform : *Ball->Transform;

  bool RenderCompletedBeforePhysicsFinished = false;
  {
    std::scoped_lock Lock(Mutex);
    RenderCompletedBeforePhysicsFinished = FirstRenderCompleted;
  }

  EXPECT_LT(PhysicsElapsed.count(), 150);
  EXPECT_FALSE(RenderCompletedBeforePhysicsFinished);
  EXPECT_LT(Transform.Location.y, 5.0f);

  Renderer.WaitForIdle();
  MeshResource.Mesh.reset();
  Renderer.Shutdown();
#endif
}
