#include "AxiomRHI/Vulkan/VulkanSceneRenderer.h"

#include "AxiomRHI/Vulkan/VulkanRhiDevice.h"

namespace Axiom {
namespace {
VulkanRhiDevice *RequireVulkanDevice(IRHIDevice &Device) {
  return dynamic_cast<VulkanRhiDevice *>(&Device);
}
} // namespace

void VulkanSceneRenderer::Init(IRHIDevice &Device,
                               const RendererCreateInfo &CreateInfo) {
  m_Device = RequireVulkanDevice(Device);
  if (m_Device != nullptr) {
    m_Device->SetViewportFrameOutput(CreateInfo.FrameOutput);
  }
}

void VulkanSceneRenderer::Shutdown() { m_Device = nullptr; }

std::shared_ptr<Mesh>
VulkanSceneRenderer::CreateMesh(const MeshData &Mesh,
                                const MeshCreateOptions &Options) {
  return m_Device != nullptr ? m_Device->CreateMesh(Mesh, Options) : nullptr;
}

MaterialHandle
VulkanSceneRenderer::CreateMaterialHandle(const MaterialInstance &Material) {
  return m_Device != nullptr ? m_Device->CreateMaterialHandle(Material)
                             : MaterialHandle{};
}

void VulkanSceneRenderer::UpdateMaterialHandle(
    MaterialHandle Handle, const MaterialInstance &Material) {
  if (m_Device != nullptr) {
    m_Device->UpdateMaterialHandle(Handle, Material);
  }
}

void VulkanSceneRenderer::Render(RenderScene &Scene) {
  if (m_Device == nullptr) {
    return;
  }

  if (!Scene.Submissions.empty()) {
    m_Device->PrepareSceneFrame(Scene);
    m_Device->RecordBackground();
    m_Device->RecordDepthPrepass();
    m_Device->BuildHzb();
    m_Device->RecordComputeMeshPath();
    m_Device->RecordOpaqueForward();
    m_Device->RecordTranslucentForward();
    m_Device->FinalizeSceneFrame();
    return;
  }

  m_Device->RenderFallbackBackground(Scene);
}

void VulkanSceneRenderer::RenderImGui() {
  if (m_Device != nullptr) {
    m_Device->RenderImGui();
  }
}

void VulkanSceneRenderer::EndFrame() {
  if (m_Device != nullptr) {
    m_Device->EndFrame();
  }
}

void VulkanSceneRenderer::SetViewMode(RendererViewMode ViewMode) {
  if (m_Device != nullptr) {
    m_Device->SetViewMode(ViewMode);
  }
}

void VulkanSceneRenderer::SetViewportFrameUser(SessionUserId User) {
  if (m_Device != nullptr) {
    m_Device->SetViewportFrameUser(User);
  }
}

void VulkanSceneRenderer::SetViewportFrameOutput(
    IViewportFrameOutput *FrameOutput) {
  if (m_Device != nullptr) {
    m_Device->SetViewportFrameOutput(FrameOutput);
  }
}

std::optional<CapturedFrame> VulkanSceneRenderer::ConsumeCapturedFrame() {
  return m_Device != nullptr ? m_Device->ConsumeCapturedFrame() : std::nullopt;
}

RendererFrameStats &VulkanSceneRenderer::AccessFrameStats() {
  return m_Device->AccessFrameStats();
}

const RendererFrameStats &VulkanSceneRenderer::GetFrameStats() const {
  return m_Device->GetFrameStats();
}
} // namespace Axiom
