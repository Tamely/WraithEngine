#include "Renderer/Renderer.h"

#include "Assets/MeshAsset.h"
#include "Renderer/ForwardRenderer.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/Vulkan/VulkanRendererBackend.h"

#include "Core/Log.h"

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

void Renderer::Render() { m_Technique->Render(m_Scene); }

void Renderer::EndFrame() {
  RenderCommand::EndScene();
  m_Backend->RenderImGui();
  m_Backend->EndFrame();
}

MeshRef Renderer::LoadMeshFromFile(const std::filesystem::path &Path) {
  auto MeshData = Assets::LoadBasicMeshAsset(Path);
  if (!MeshData.has_value()) {
    A_CORE_ERROR("Failed to load mesh asset: {0}", Path.string());
    return {};
  }

  return m_Backend->CreateMesh(*MeshData);
}
} // namespace Axiom
