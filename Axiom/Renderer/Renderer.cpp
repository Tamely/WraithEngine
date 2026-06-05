#include "Renderer/Renderer.h"

#include "Assets/MeshAsset.h"
#include "Core/Threading.h"
#include "Renderer/RenderCommand.h"
#include "RHI/RHIFactory.h"

#include "Core/Log.h"

#include <cassert>
#include <chrono>
#include <future>
#include <stdexcept>
#include <type_traits>

namespace Axiom {
namespace {
constexpr size_t kInvalidSceneIndex = static_cast<size_t>(-1);
}

Renderer::~Renderer() { Shutdown(); }

void Renderer::Init(const RendererCreateInfo &CreateInfo) {
  if (m_IsInitialized) {
    return;
  }

  m_CreateInfo = CreateInfo;
  m_AttachmentRequirements = CreateInfo.AttachmentRequirements;
  m_EnableThreadedRendering = CreateInfo.EnableThreadedRendering;
  m_PendingCpuFrameMs = 0.0f;
  m_NextFrameNumber = 0;
  m_LastKnownFrameStats = {};
  m_FreeSceneIndices.clear();
  m_QueuedSceneIndices.clear();
  m_RecordingSceneIndex = kInvalidSceneIndex;
  m_RenderThreadFailure.reset();
  m_RenderThreadReady = false;
  m_RenderThreadExitRequested = false;

  if (IsThreadedRenderEnabled()) {
    StartThreadedRenderer(CreateInfo);
  } else {
    InitializeBackendOnCurrentThread(CreateInfo);
  }

  m_IsInitialized = true;
}

void Renderer::Shutdown() {
  if (!m_IsInitialized) {
    return;
  }

  if (m_RecordingSceneIndex != kInvalidSceneIndex) {
    RenderCommand::EndScene();
    m_Scenes[m_RecordingSceneIndex].Reset();
    m_RecordingSceneIndex = kInvalidSceneIndex;
  }

  if (IsThreadedRenderEnabled()) {
    StopThreadedRenderer();
  } else {
    for (RenderScene &Scene : m_Scenes) {
      Scene.Reset();
    }
    ShutdownBackendOnCurrentThread();
  }

  m_CreateInfo.reset();
  m_AttachmentRequirements = {};
  m_EnableThreadedRendering = false;
  m_RenderThreadFailure.reset();
  m_LastKnownFrameStats = {};
  m_IsInitialized = false;
}

void Renderer::BeginFrame() {
  if (IsThreadedRenderEnabled()) {
    RenderScene &Scene = AcquireRecordingScene();
    Scene.FrameNumber = ++m_NextFrameNumber;
    Scene.CpuFrameMs = m_PendingCpuFrameMs;
    RenderCommand::BeginScene(Scene);
    return;
  }

  RenderScene &Scene = m_Scenes[0];
  Scene.Reset();
  Scene.FrameNumber = ++m_NextFrameNumber;
  Scene.CpuFrameMs = m_PendingCpuFrameMs;
  if (m_SceneRenderer != nullptr) {
    m_SceneRenderer->BeginFrame();
  }
  RenderCommand::BeginScene(Scene);
}

void Renderer::Render() {
  if (IsThreadedRenderEnabled()) {
    return;
  }

  if (m_SceneRenderer == nullptr) {
    return;
  }

  const auto StartTime = std::chrono::steady_clock::now();
  m_SceneRenderer->Render(m_Scenes[0]);
  const auto EndTime = std::chrono::steady_clock::now();
  UpdateCpuRenderTime(
      std::chrono::duration<float, std::milli>(EndTime - StartTime).count());
}

void Renderer::EndFrame() {
  RenderCommand::EndScene();

  if (IsThreadedRenderEnabled()) {
    const size_t SceneIndex = m_RecordingSceneIndex;
    m_RecordingSceneIndex = kInvalidSceneIndex;
    {
      std::scoped_lock Lock(m_RenderThreadMutex);
      m_QueuedSceneIndices.push_back(SceneIndex);
    }
    m_RenderThreadCv.notify_all();
    return;
  }

  if (m_SceneRenderer != nullptr) {
    m_SceneRenderer->RenderImGui();
    m_SceneRenderer->EndFrame();
    m_LastKnownFrameStats = m_SceneRenderer->GetFrameStats();
  }
}

void Renderer::SetViewMode(RendererViewMode ViewMode) {
  if (IsThreadedRenderEnabled()) {
    InvokeOnRenderThread([this, ViewMode]() {
      if (m_SceneRenderer != nullptr) {
        m_SceneRenderer->SetViewMode(ViewMode);
      }
    });
    return;
  }

  if (m_SceneRenderer != nullptr) {
    m_SceneRenderer->SetViewMode(ViewMode);
  }
}

void Renderer::SetViewportFrameUser(SessionUserId User) {
  if (IsThreadedRenderEnabled()) {
    InvokeOnRenderThread([this, User]() {
      if (m_SceneRenderer != nullptr) {
        m_SceneRenderer->SetViewportFrameUser(User);
      }
    });
    return;
  }

  if (m_SceneRenderer != nullptr) {
    m_SceneRenderer->SetViewportFrameUser(User);
  }
}

void Renderer::SetViewportFrameOutput(IViewportFrameOutput *FrameOutput) {
  if (IsThreadedRenderEnabled()) {
    InvokeOnRenderThread([this, FrameOutput]() {
      if (m_SceneRenderer != nullptr) {
        m_SceneRenderer->SetViewportFrameOutput(FrameOutput);
      }
    });
    return;
  }

  if (m_SceneRenderer != nullptr) {
    m_SceneRenderer->SetViewportFrameOutput(FrameOutput);
  }
}

std::optional<CapturedFrame> Renderer::ConsumeCapturedFrame() {
  if (IsThreadedRenderEnabled()) {
    return InvokeOnRenderThread<std::optional<CapturedFrame>>([this]() {
      return m_SceneRenderer != nullptr ? m_SceneRenderer->ConsumeCapturedFrame()
                                        : std::nullopt;
    });
  }

  return m_SceneRenderer != nullptr ? m_SceneRenderer->ConsumeCapturedFrame()
                                    : std::nullopt;
}

void Renderer::SetCpuFrameTime(float CpuFrameMs) {
  m_PendingCpuFrameMs = CpuFrameMs;
  m_LastKnownFrameStats.CpuFrameMs = CpuFrameMs;
  if (!IsThreadedRenderEnabled() && m_SceneRenderer != nullptr) {
    m_SceneRenderer->AccessFrameStats().CpuFrameMs = CpuFrameMs;
  }
}

RendererFrameStats Renderer::GetFrameStats() const {
  if (IsThreadedRenderEnabled()) {
    return const_cast<Renderer *>(this)->InvokeOnRenderThread<RendererFrameStats>(
        [this]() {
          return m_SceneRenderer != nullptr ? m_SceneRenderer->GetFrameStats()
                                            : RendererFrameStats{};
        });
  }

  return m_SceneRenderer != nullptr ? m_SceneRenderer->GetFrameStats()
                                    : m_LastKnownFrameStats;
}

void Renderer::WaitForIdle() {
  if (IsThreadedRenderEnabled()) {
    {
      std::unique_lock Lock(m_RenderThreadMutex);
      m_RenderThreadCv.wait(Lock, [this]() {
        return m_QueuedSceneIndices.empty() &&
               m_FreeSceneIndices.size() == m_Scenes.size() &&
               m_RecordingSceneIndex == kInvalidSceneIndex;
      });
    }
    InvokeOnRenderThread([this]() {
      if (m_RhiDevice != nullptr) {
        m_RhiDevice->WaitIdle();
      }
    });
    return;
  }

  if (m_RhiDevice != nullptr) {
    m_RhiDevice->WaitIdle();
  }
}

std::shared_ptr<Mesh> Renderer::CreateMesh(const MeshData &MeshData,
                                           const MeshCreateOptions &Options) {
  if (IsThreadedRenderEnabled()) {
    return InvokeOnRenderThread<std::shared_ptr<Mesh>>([this, MeshData, Options]() {
      return m_SceneRenderer != nullptr ? m_SceneRenderer->CreateMesh(MeshData, Options)
                                        : nullptr;
    });
  }

  return m_SceneRenderer != nullptr ? m_SceneRenderer->CreateMesh(MeshData, Options)
                                    : nullptr;
}

MaterialHandle Renderer::CreateMaterialHandle(const MaterialInstance &Material) {
  if (IsThreadedRenderEnabled()) {
    return InvokeOnRenderThread<MaterialHandle>([this, Material]() {
      return m_SceneRenderer != nullptr
                 ? m_SceneRenderer->CreateMaterialHandle(Material)
                 : MaterialHandle{};
    });
  }

  return m_SceneRenderer != nullptr
             ? m_SceneRenderer->CreateMaterialHandle(Material)
             : MaterialHandle{};
}

void Renderer::UpdateMaterialHandle(MaterialHandle Handle,
                                    const MaterialInstance &Material) {
  if (m_SceneRenderer == nullptr || !Handle.IsValid()) {
    return;
  }

  if (IsThreadedRenderEnabled()) {
    InvokeOnRenderThread([this, Handle, Material]() {
      if (m_SceneRenderer != nullptr) {
        m_SceneRenderer->UpdateMaterialHandle(Handle, Material);
      }
    });
    return;
  }

  m_SceneRenderer->UpdateMaterialHandle(Handle, Material);
}

RenderMeshResource Renderer::CreateMeshResource(const MeshData &MeshData,
                                                const MeshCreateOptions &Options) {
  RenderMeshResource Resource;
  Resource.Mesh = CreateMesh(MeshData, Options);
  Resource.Handle = GetMeshHandle(Resource.Mesh);
  if (Resource.Mesh != nullptr) {
    assert(Resource.Handle.IsValid() &&
           "Renderer backend returned a mesh without a valid handle");
  }
  return Resource;
}

void Renderer::UpdateCpuRenderTime(float CpuRenderMs) {
  m_LastKnownFrameStats.CpuRenderMs = CpuRenderMs;
  if (m_SceneRenderer != nullptr) {
    m_SceneRenderer->AccessFrameStats().CpuRenderMs = CpuRenderMs;
  }
}

LoadedMeshScene
Renderer::LoadMeshSceneFromFile(const std::filesystem::path &Path,
                                const MeshSceneLoadOptions &Options) {
  auto SceneData = Assets::LoadBasicMeshAsset(Path);
  if (!SceneData.has_value()) {
    A_CORE_ERROR("Failed to load mesh asset scene: {0}", Path.string());
    return {};
  }

  LoadedMeshScene Result;
  Result.Resources.reserve(SceneData->Instances.size());
  Result.Submissions.reserve(SceneData->Instances.size());
  for (const auto &Instance : SceneData->Instances) {
    RenderMeshResource Resource = CreateMeshResource(Instance.Mesh);
    if (!Resource.IsValid()) {
      continue;
    }

    const MeshRenderPath RenderPath =
        Options.ComputeMeshNames.contains(Instance.Name)
            ? MeshRenderPath::Compute
            : Options.DefaultRenderPath;
    Result.Resources.push_back(Resource);
    Result.Submissions.push_back(
        {.MeshHandle = Result.Resources.back().Handle,
         .MaterialHandle =
             Instance.Material != nullptr
                 ? CreateMaterialHandle(*Instance.Material)
                 : MaterialHandle{},
         .DebugDataId = RegisterRenderMeshSubmissionDebugData(
             {.Name = Instance.Name}),
         .RenderPath = RenderPath,
         .Transform = Instance.Transform});
  }

  return Result;
}

template <typename Result>
Result Renderer::InvokeOnRenderThread(std::function<Result()> Function) {
  if (!IsThreadedRenderEnabled()) {
    return Function();
  }

  auto Promise = std::make_shared<std::promise<Result>>();
  std::future<Result> Future = Promise->get_future();
  {
    std::scoped_lock Lock(m_RenderThreadMutex);
    m_RenderThreadCommands.push_back(
        [Promise, Function = std::move(Function)]() mutable {
          try {
            if constexpr (std::is_void_v<Result>) {
              Function();
              Promise->set_value();
            } else {
              Promise->set_value(Function());
            }
          } catch (...) {
            Promise->set_exception(std::current_exception());
          }
        });
  }
  m_RenderThreadCv.notify_all();

  if constexpr (std::is_void_v<Result>) {
    Future.get();
  } else {
    return Future.get();
  }
}

void Renderer::InvokeOnRenderThread(std::function<void()> Function) {
  InvokeOnRenderThread<void>(std::move(Function));
}

void Renderer::StartThreadedRenderer(const RendererCreateInfo &CreateInfo) {
  for (size_t SceneIndex = 0; SceneIndex < m_Scenes.size(); ++SceneIndex) {
    m_Scenes[SceneIndex].Reset();
    m_FreeSceneIndices.push_back(SceneIndex);
  }

  m_RenderThread = std::thread([this]() { RenderThreadMain(); });

  std::unique_lock Lock(m_RenderThreadMutex);
  m_RenderThreadCv.wait(Lock, [this]() {
    return m_RenderThreadReady || m_RenderThreadFailure.has_value();
  });
  if (m_RenderThreadFailure.has_value()) {
    throw std::runtime_error(*m_RenderThreadFailure);
  }

  (void)CreateInfo;
}

void Renderer::StopThreadedRenderer() {
  {
    std::unique_lock Lock(m_RenderThreadMutex);
    m_RenderThreadCv.wait(Lock, [this]() {
      return m_QueuedSceneIndices.empty() &&
             m_FreeSceneIndices.size() == m_Scenes.size() &&
             m_RecordingSceneIndex == kInvalidSceneIndex &&
             m_RenderThreadCommands.empty();
    });
    m_RenderThreadExitRequested = true;
  }
  m_RenderThreadCv.notify_all();

  if (m_RenderThread.joinable()) {
    m_RenderThread.join();
  }

  std::scoped_lock Lock(m_RenderThreadMutex);
  m_FreeSceneIndices.clear();
  m_QueuedSceneIndices.clear();
  m_RenderThreadCommands.clear();
  m_RenderThreadReady = false;
  m_RenderThreadExitRequested = false;
}

void Renderer::RenderThreadMain() {
  try {
    Threading::SetCurrentThreadName("Axiom Render Thread");

    if (!m_CreateInfo.has_value()) {
      throw std::runtime_error("Renderer create info missing for render thread");
    }

    InitializeBackendOnCurrentThread(*m_CreateInfo);

    {
      std::scoped_lock Lock(m_RenderThreadMutex);
      m_RenderThreadReady = true;
    }
    m_RenderThreadCv.notify_all();

    while (true) {
      std::function<void()> Command;
      size_t SceneIndex = kInvalidSceneIndex;
      {
        std::unique_lock Lock(m_RenderThreadMutex);
        m_RenderThreadCv.wait(Lock, [this]() {
          return m_RenderThreadExitRequested || !m_RenderThreadCommands.empty() ||
                 !m_QueuedSceneIndices.empty();
        });

        if (!m_QueuedSceneIndices.empty()) {
          SceneIndex = m_QueuedSceneIndices.front();
          m_QueuedSceneIndices.pop_front();
        } else if (!m_RenderThreadCommands.empty()) {
          Command = std::move(m_RenderThreadCommands.front());
          m_RenderThreadCommands.pop_front();
        } else if (m_RenderThreadExitRequested) {
          break;
        }
      }

      if (Command) {
        Command();
        m_RenderThreadCv.notify_all();
        continue;
      }

      if (SceneIndex != kInvalidSceneIndex) {
        RenderSceneOnCurrentThread(m_Scenes[SceneIndex]);
        ReleaseRenderedScene(SceneIndex);
      }
    }

    ShutdownBackendOnCurrentThread();
  } catch (const std::exception &Error) {
    std::scoped_lock Lock(m_RenderThreadMutex);
    m_RenderThreadFailure = Error.what();
    m_RenderThreadReady = true;
    m_RenderThreadCv.notify_all();
  }
}

void Renderer::InitializeBackendOnCurrentThread(
    const RendererCreateInfo &CreateInfo) {
  m_RhiDevice = CreateRHIDevice(CreateInfo.BackendType);
  assert(m_RhiDevice != nullptr && "RHI device factory returned null");
  if (m_RhiDevice == nullptr) {
    return;
  }

  m_RhiDevice->Init({.TargetSurface = CreateInfo.TargetSurface,
                     .FrameOutput = CreateInfo.FrameOutput,
                     .Width = CreateInfo.Width,
                     .Height = CreateInfo.Height,
                     .AttachmentRequirements = m_AttachmentRequirements});
  m_SceneRenderer =
      std::make_unique<SceneRenderer>(*m_RhiDevice, CreateInfo.BackendType);
  m_SceneRenderer->Init(CreateInfo);
}

void Renderer::ShutdownBackendOnCurrentThread() {
  for (RenderScene &Scene : m_Scenes) {
    Scene.Reset();
  }

  if (m_RhiDevice != nullptr) {
    m_RhiDevice->WaitIdle();
  }
  if (m_SceneRenderer != nullptr) {
    m_SceneRenderer->Shutdown();
    m_SceneRenderer.reset();
  }
  if (m_RhiDevice != nullptr) {
    m_RhiDevice->Shutdown();
    m_RhiDevice.reset();
  }
}

void Renderer::RenderSceneOnCurrentThread(RenderScene &Scene) {
  if (m_SceneRenderer == nullptr) {
    return;
  }

  if (m_CreateInfo.has_value() && m_CreateInfo->ThreadedRenderSceneStartCallback) {
    m_CreateInfo->ThreadedRenderSceneStartCallback(Scene.FrameNumber);
  }

  const auto StartTime = std::chrono::steady_clock::now();
  m_SceneRenderer->BeginFrame();
  m_SceneRenderer->AccessFrameStats().CpuFrameMs = Scene.CpuFrameMs;
  m_SceneRenderer->Render(Scene);
  // Threaded rendering currently skips renderer-owned ImGui composition so the
  // game thread can stay authoritative for UI/input state.
  m_SceneRenderer->EndFrame();
  const auto EndTime = std::chrono::steady_clock::now();
  m_SceneRenderer->AccessFrameStats().CpuRenderMs =
      std::chrono::duration<float, std::milli>(EndTime - StartTime).count();
  m_LastKnownFrameStats = m_SceneRenderer->GetFrameStats();

  if (m_CreateInfo.has_value() &&
      m_CreateInfo->ThreadedRenderSceneCompleteCallback) {
    m_CreateInfo->ThreadedRenderSceneCompleteCallback(Scene.FrameNumber);
  }
}

RenderScene &Renderer::AcquireRecordingScene() {
  std::unique_lock Lock(m_RenderThreadMutex);
  m_RenderThreadCv.wait(Lock, [this]() { return !m_FreeSceneIndices.empty(); });
  m_RecordingSceneIndex = m_FreeSceneIndices.front();
  m_FreeSceneIndices.pop_front();
  RenderScene &Scene = m_Scenes[m_RecordingSceneIndex];
  Scene.Reset();
  return Scene;
}

void Renderer::ReleaseRenderedScene(size_t SceneIndex) {
  {
    std::scoped_lock Lock(m_RenderThreadMutex);
    m_Scenes[SceneIndex].Reset();
    m_FreeSceneIndices.push_back(SceneIndex);
  }
  m_RenderThreadCv.notify_all();
}

bool Renderer::IsThreadedRenderEnabled() const {
  return AXIOM_THREADED_RENDER != 0 && m_EnableThreadedRendering;
}
} // namespace Axiom
