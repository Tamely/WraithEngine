#include "Renderer/Renderer.h"

#include "Assets/MeshAsset.h"
#include "Renderer/ForwardRenderer.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/Vulkan/VulkanRendererBackend.h"

#include "Core/Log.h"

#include <chrono>

namespace Axiom {
Renderer *Renderer::s_Instance = nullptr;

Renderer::~Renderer() { Shutdown(); }

Renderer &Renderer::Get() { return *s_Instance; }

void Renderer::Init(const RendererCreateInfo &CreateInfo) {
  if (m_IsInitialized) {
    return;
  }

  switch (CreateInfo.BackendType) {
  case RendererBackendType::Vulkan:
    m_Backend = std::make_unique<VulkanRendererBackend>();
    break;
  }

  m_Backend->Init(CreateInfo);
  m_Technique = std::make_unique<ForwardRenderer>();
  m_Technique->Init(m_Backend.get());
  s_Instance = this;
  m_IsInitialized = true;
}

void Renderer::Shutdown() {
  if (!m_IsInitialized) {
    return;
  }

  m_Technique->Shutdown();
  m_Technique.reset();
  m_Backend->Shutdown();
  m_Backend.reset();
  if (s_Instance == this) {
    s_Instance = nullptr;
  }
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

void Renderer::SetCpuFrameTime(float CpuFrameMs) {
  m_Backend->AccessFrameStats().CpuFrameMs = CpuFrameMs;
}

const RendererFrameStats &Renderer::GetFrameStats() const {
  return m_Backend->GetFrameStats();
}

void Renderer::UpdateCpuRenderTime(float CpuRenderMs) {
  m_Backend->AccessFrameStats().CpuRenderMs = CpuRenderMs;
}

std::vector<RenderMeshSubmission>
Renderer::LoadMeshSceneFromFile(const std::filesystem::path &Path,
                                const MeshSceneLoadOptions &Options) {
  auto SceneData = Assets::LoadBasicMeshAsset(Path);
  if (!SceneData.has_value()) {
    A_CORE_ERROR("Failed to load mesh asset scene: {0}", Path.string());
    return {};
  }

  std::vector<RenderMeshSubmission> Result;
  Result.reserve(SceneData->Instances.size());
  for (const auto &Instance : SceneData->Instances) {
    auto Mesh = m_Backend->CreateMesh(Instance.Mesh);
    if (!Mesh) {
      continue;
    }

    const MeshRenderPath RenderPath =
        Options.ComputeMeshNames.contains(Instance.Name)
            ? MeshRenderPath::Compute
            : Options.DefaultRenderPath;
    Result.push_back(
        {.Mesh = Mesh,
         .Name = Instance.Name,
         .RenderPath = RenderPath,
         .Transform = Instance.Transform});
  }

  return Result;
}
} // namespace Axiom
