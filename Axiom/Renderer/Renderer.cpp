#include "Renderer/Renderer.h"

#include "Assets/MeshAsset.h"
#include "Renderer/RenderCommand.h"
#include "RHI/RHIFactory.h"

#include "Core/Log.h"

#include <chrono>

namespace Axiom {
Renderer::~Renderer() { Shutdown(); }

void Renderer::Init(const RendererCreateInfo &CreateInfo) {
  if (m_IsInitialized) {
    return;
  }

  m_CreateInfo = CreateInfo;
  m_AttachmentRequirements = CreateInfo.AttachmentRequirements;
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
  m_IsInitialized = true;
}

void Renderer::Shutdown() {
  if (!m_IsInitialized) {
    return;
  }

  m_Scene.Reset();
  if (m_SceneRenderer != nullptr) {
    m_SceneRenderer->Shutdown();
    m_SceneRenderer.reset();
  }
  m_RhiDevice->Shutdown();
  m_RhiDevice.reset();
  m_CreateInfo.reset();
  m_AttachmentRequirements = {};
  m_IsInitialized = false;
}

void Renderer::BeginFrame() {
  m_RhiDevice->BeginFrame();
  RenderCommand::BeginScene(m_Scene);
}

void Renderer::Render() {
  const auto StartTime = std::chrono::steady_clock::now();
  m_SceneRenderer->Render(m_Scene);
  const auto EndTime = std::chrono::steady_clock::now();
  UpdateCpuRenderTime(
      std::chrono::duration<float, std::milli>(EndTime - StartTime).count());
}

void Renderer::EndFrame() {
  RenderCommand::EndScene();
  m_SceneRenderer->RenderImGui();
  m_SceneRenderer->EndFrame();
}

void Renderer::SetViewMode(RendererViewMode ViewMode) {
  if (m_SceneRenderer != nullptr) {
    m_SceneRenderer->SetViewMode(ViewMode);
  }
}

void Renderer::SetViewportFrameUser(SessionUserId User) {
  if (m_SceneRenderer != nullptr) {
    m_SceneRenderer->SetViewportFrameUser(User);
  }
}

void Renderer::SetViewportFrameOutput(IViewportFrameOutput *FrameOutput) {
  if (m_SceneRenderer != nullptr) {
    m_SceneRenderer->SetViewportFrameOutput(FrameOutput);
  }
}

std::optional<CapturedFrame> Renderer::ConsumeCapturedFrame() {
  return m_SceneRenderer != nullptr ? m_SceneRenderer->ConsumeCapturedFrame()
                                    : std::nullopt;
}

void Renderer::SetCpuFrameTime(float CpuFrameMs) {
  m_SceneRenderer->AccessFrameStats().CpuFrameMs = CpuFrameMs;
}

const RendererFrameStats &Renderer::GetFrameStats() const {
  return m_SceneRenderer->GetFrameStats();
}

std::shared_ptr<Mesh> Renderer::CreateMesh(const MeshData &MeshData,
                                           const MeshCreateOptions &Options) {
  return m_SceneRenderer != nullptr ? m_SceneRenderer->CreateMesh(MeshData, Options)
                                    : nullptr;
}

MaterialHandle Renderer::CreateMaterialHandle(const MaterialInstance &Material) {
  return m_SceneRenderer != nullptr
             ? m_SceneRenderer->CreateMaterialHandle(Material)
             : MaterialHandle{};
}

void Renderer::UpdateMaterialHandle(MaterialHandle Handle,
                                    const MaterialInstance &Material) {
  if (m_SceneRenderer == nullptr || !Handle.IsValid()) {
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
  m_SceneRenderer->AccessFrameStats().CpuRenderMs = CpuRenderMs;
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
} // namespace Axiom
