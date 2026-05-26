#include "Renderer/Renderer.h"

#include "Assets/MeshAsset.h"
#include "Renderer/ForwardRenderer.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/Vulkan/VulkanRendererBackend.h"

#include "Core/Log.h"

#include <chrono>

namespace Axiom {
std::unique_ptr<RenderTechnique>
Renderer::CreateTechnique(RendererTechniqueType TechniqueType) {
  switch (TechniqueType) {
  case RendererTechniqueType::Forward:
    return std::make_unique<ForwardRenderer>();
  }

  return std::make_unique<ForwardRenderer>();
}

Renderer::~Renderer() { Shutdown(); }

void Renderer::Init(const RendererCreateInfo &CreateInfo) {
  if (m_IsInitialized) {
    return;
  }

  m_CreateInfo = CreateInfo;
  m_Technique = CreateTechnique(CreateInfo.Technique);
  m_AttachmentRequirements = m_Technique->GetAttachmentRequirements();

  switch (CreateInfo.BackendType) {
  case RendererBackendType::Vulkan:
    m_Backend = std::make_unique<VulkanRendererBackend>();
    break;
  }

  RendererCreateInfo BackendCreateInfo = CreateInfo;
  BackendCreateInfo.AttachmentRequirements = m_AttachmentRequirements;
  m_Backend->Init(BackendCreateInfo);
  m_Technique->Init(*m_Backend);
  m_IsInitialized = true;
}

void Renderer::Shutdown() {
  if (!m_IsInitialized) {
    return;
  }

  m_Scene.Reset();
  if (m_Technique != nullptr) {
    m_Technique->Shutdown();
    m_Technique.reset();
  }
  m_Backend->Shutdown();
  m_Backend.reset();
  m_CreateInfo.reset();
  m_AttachmentRequirements = {};
  m_IsInitialized = false;
}

void Renderer::BeginFrame() {
  m_Backend->BeginFrame();
  RenderCommand::BeginScene(m_Scene);
}

void Renderer::Render() {
  const auto StartTime = std::chrono::steady_clock::now();
  m_Technique->Render(m_Scene);
  const auto EndTime = std::chrono::steady_clock::now();
  UpdateCpuRenderTime(
      std::chrono::duration<float, std::milli>(EndTime - StartTime).count());
}

void Renderer::EndFrame() {
  RenderCommand::EndScene();
  m_Backend->RenderImGui();
  m_Backend->EndFrame();
}

void Renderer::SetTechnique(std::unique_ptr<RenderTechnique> Technique) {
  assert(Technique != nullptr && "Renderer requires a valid render technique");
  if (Technique == nullptr) {
    return;
  }

  const RenderTechnique::AttachmentRequirements NewRequirements =
      Technique->GetAttachmentRequirements();
  if (m_Backend != nullptr && m_Technique != nullptr &&
      NewRequirements != m_AttachmentRequirements) {
    A_CORE_WARN(
        "Switching render techniques with different attachment requirements is "
        "not supported without renderer reinitialization yet.");
  }

  if (m_Technique != nullptr) {
    m_Technique->Shutdown();
  }

  m_Technique = std::move(Technique);
  m_AttachmentRequirements = NewRequirements;

  if (m_Backend != nullptr) {
    m_Technique->Init(*m_Backend);
  }
}

void Renderer::SetViewMode(RendererViewMode ViewMode) {
  if (m_Backend != nullptr) {
    m_Backend->SetViewMode(ViewMode);
  }
}

void Renderer::SetViewportFrameUser(SessionUserId User) {
  if (m_Backend != nullptr) {
    m_Backend->SetViewportFrameUser(User);
  }
}

void Renderer::SetViewportFrameOutput(IViewportFrameOutput *FrameOutput) {
  if (m_Backend != nullptr) {
    m_Backend->SetViewportFrameOutput(FrameOutput);
  }
}

std::optional<CapturedFrame> Renderer::ConsumeCapturedFrame() {
  return m_Backend->ConsumeCapturedFrame();
}

void Renderer::SetCpuFrameTime(float CpuFrameMs) {
  m_Backend->AccessFrameStats().CpuFrameMs = CpuFrameMs;
}

const RendererFrameStats &Renderer::GetFrameStats() const {
  return m_Backend->GetFrameStats();
}

std::shared_ptr<Mesh> Renderer::CreateMesh(const MeshData &MeshData,
                                           const MeshCreateOptions &Options) {
  return m_Backend != nullptr ? m_Backend->CreateMesh(MeshData, Options)
                              : nullptr;
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
  m_Backend->AccessFrameStats().CpuRenderMs = CpuRenderMs;
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
         .Material = Instance.Material,
         .DebugDataId = RegisterRenderMeshSubmissionDebugData(
             {.Name = Instance.Name}),
         .RenderPath = RenderPath,
         .Transform = Instance.Transform});
  }

  return Result;
}
} // namespace Axiom
