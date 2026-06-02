#pragma once

#include "Renderer/RendererBackend.h"
#include "Renderer/RenderScene.h"
#include "Renderer/RenderSurface.h"
#include "Renderer/RenderTechnique.h"

#include <filesystem>
#include <memory>
#include <optional>

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
  void SetTechnique(std::unique_ptr<RenderTechnique> Technique);
  void SetViewMode(RendererViewMode ViewMode);
  void SetViewportFrameUser(SessionUserId User);
  void SetViewportFrameOutput(IViewportFrameOutput *FrameOutput);
  std::optional<CapturedFrame> ConsumeCapturedFrame();
  void SetCpuFrameTime(float CpuFrameMs);
  const RendererFrameStats &GetFrameStats() const;
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
  static std::unique_ptr<RenderTechnique>
  CreateTechnique(RendererTechniqueType TechniqueType);
  void UpdateCpuRenderTime(float CpuRenderMs);

private:
  std::unique_ptr<RendererBackend> m_Backend;
  std::unique_ptr<RenderTechnique> m_Technique;
  RenderTechnique::AttachmentRequirements m_AttachmentRequirements{};
  std::optional<RendererCreateInfo> m_CreateInfo;
  RenderScene m_Scene;
  bool m_IsInitialized{false};
};
} // namespace Axiom
