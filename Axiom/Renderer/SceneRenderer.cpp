#include "Renderer/SceneRenderer.h"

#include "AxiomRHI/SceneRendererBackendFactory.h"

namespace Axiom {
SceneRenderer::SceneRenderer(IRHIDevice &Device, RendererBackendType BackendType)
    : m_Device(Device), m_BackendType(BackendType) {}

SceneRenderer::~SceneRenderer() { Shutdown(); }

void SceneRenderer::Init(const RendererCreateInfo &CreateInfo) {
  if (m_Backend != nullptr) {
    return;
  }

  m_Backend = CreateSceneRendererBackend(m_Device, m_BackendType);
  if (m_Backend != nullptr) {
    m_Backend->Init(m_Device, CreateInfo);
  }
}

void SceneRenderer::Shutdown() {
  if (m_Backend == nullptr) {
    return;
  }

  m_Backend->Shutdown();
  m_Backend.reset();
}

std::shared_ptr<Mesh>
SceneRenderer::CreateMesh(const MeshData &MeshData,
                          const MeshCreateOptions &Options) {
  return m_Backend != nullptr ? m_Backend->CreateMesh(MeshData, Options)
                              : nullptr;
}

MaterialHandle
SceneRenderer::CreateMaterialHandle(const MaterialInstance &Material) {
  return m_Backend != nullptr ? m_Backend->CreateMaterialHandle(Material)
                              : MaterialHandle{};
}

void SceneRenderer::UpdateMaterialHandle(MaterialHandle Handle,
                                         const MaterialInstance &Material) {
  if (m_Backend != nullptr) {
    m_Backend->UpdateMaterialHandle(Handle, Material);
  }
}

void SceneRenderer::Render(RenderScene &Scene) {
  if (m_Backend != nullptr) {
    m_Backend->Render(Scene);
  }
}

void SceneRenderer::RenderImGui() {
  if (m_Backend != nullptr) {
    m_Backend->RenderImGui();
  }
}

void SceneRenderer::EndFrame() {
  if (m_Backend != nullptr) {
    m_Backend->EndFrame();
  }
}

void SceneRenderer::SetViewMode(RendererViewMode ViewMode) {
  if (m_Backend != nullptr) {
    m_Backend->SetViewMode(ViewMode);
  }
}

void SceneRenderer::SetViewportFrameUser(SessionUserId User) {
  if (m_Backend != nullptr) {
    m_Backend->SetViewportFrameUser(User);
  }
}

void SceneRenderer::SetViewportFrameOutput(IViewportFrameOutput *FrameOutput) {
  if (m_Backend != nullptr) {
    m_Backend->SetViewportFrameOutput(FrameOutput);
  }
}

std::optional<CapturedFrame> SceneRenderer::ConsumeCapturedFrame() {
  return m_Backend != nullptr ? m_Backend->ConsumeCapturedFrame()
                              : std::nullopt;
}

RendererFrameStats &SceneRenderer::AccessFrameStats() {
  return m_Backend->AccessFrameStats();
}

const RendererFrameStats &SceneRenderer::GetFrameStats() const {
  return m_Backend->GetFrameStats();
}
} // namespace Axiom
