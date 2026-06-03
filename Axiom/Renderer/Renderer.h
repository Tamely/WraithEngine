#pragma once

#include "Renderer/RenderScene.h"
#include "Renderer/RendererTypes.h"
#include "Renderer/SceneRenderer.h"
#include "RHI/IRHI.h"

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
  void UpdateCpuRenderTime(float CpuRenderMs);

private:
  std::unique_ptr<IRHIDevice> m_RhiDevice;
  std::unique_ptr<SceneRenderer> m_SceneRenderer;
  RendererAttachmentRequirements m_AttachmentRequirements{};
  std::optional<RendererCreateInfo> m_CreateInfo;
  RenderScene m_Scene;
  bool m_IsInitialized{false};
};
} // namespace Axiom
