#pragma once

#include "Renderer/RenderScene.h"
#include "Renderer/RendererTypes.h"
#include "Renderer/SceneRenderer.h"
#include "RHI/IRHI.h"

#include <array>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <functional>
#include <mutex>
#include <memory>
#include <optional>
#include <string>
#include <thread>

namespace Axiom {
class Renderer {
public:
  Renderer() = default;
  ~Renderer();

  Renderer(const Renderer &) = delete;
  Renderer &operator=(const Renderer &) = delete;

  void Init(const RendererCreateInfo &CreateInfo);
  void Shutdown();
  void BeginFrame();
  void Render();
  void EndFrame();
  void SetViewMode(RendererViewMode ViewMode);
  void SetViewportFrameUser(SessionUserId User);
  void SetViewportFrameOutput(IViewportFrameOutput *FrameOutput);
  std::optional<CapturedFrame> ConsumeCapturedFrame();
  void SetCpuFrameTime(float CpuFrameMs);
  RendererFrameStats GetFrameStats() const;
  void WaitForIdle();
  std::shared_ptr<Mesh>
  CreateMesh(const MeshData &MeshData, const MeshCreateOptions &Options = {});
  MaterialHandle CreateMaterialHandle(const MaterialInstance &Material);
  void UpdateMaterialHandle(MaterialHandle Handle,
                            const MaterialInstance &Material);
  RenderMeshResource
  CreateMeshResource(const MeshData &MeshData,
                     const MeshCreateOptions &Options = {});
  LoadedMeshScene
  LoadMeshSceneFromFile(
      const std::filesystem::path &Path,
      const MeshSceneLoadOptions &Options = {});

private:
  template <typename Result>
  Result InvokeOnRenderThread(std::function<Result()> Function);
  void InvokeOnRenderThread(std::function<void()> Function);
  void StartThreadedRenderer(const RendererCreateInfo &CreateInfo);
  void StopThreadedRenderer();
  void RenderThreadMain();
  void InitializeBackendOnCurrentThread(const RendererCreateInfo &CreateInfo);
  void ShutdownBackendOnCurrentThread();
  void RenderSceneOnCurrentThread(RenderScene &Scene);
  RenderScene &AcquireRecordingScene();
  void ReleaseRenderedScene(size_t SceneIndex);
  void UpdateCpuRenderTime(float CpuRenderMs);
  bool IsThreadedRenderEnabled() const;

private:
  std::unique_ptr<IRHIDevice> m_RhiDevice;
  std::unique_ptr<SceneRenderer> m_SceneRenderer;
  RendererAttachmentRequirements m_AttachmentRequirements{};
  std::optional<RendererCreateInfo> m_CreateInfo;
  std::array<RenderScene, 3> m_Scenes;
  std::deque<size_t> m_FreeSceneIndices;
  std::deque<size_t> m_QueuedSceneIndices;
  std::deque<std::function<void()>> m_RenderThreadCommands;
  mutable std::mutex m_RenderThreadMutex;
  std::condition_variable m_RenderThreadCv;
  std::thread m_RenderThread;
  size_t m_RecordingSceneIndex{static_cast<size_t>(-1)};
  bool m_EnableThreadedRendering{false};
  bool m_RenderThreadReady{false};
  bool m_RenderThreadExitRequested{false};
  std::optional<std::string> m_RenderThreadFailure;
  float m_PendingCpuFrameMs{0.0f};
  uint64_t m_NextFrameNumber{0};
  RendererFrameStats m_LastKnownFrameStats{};
  bool m_IsInitialized{false};
};
} // namespace Axiom
