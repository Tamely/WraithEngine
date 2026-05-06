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

  static Renderer &Get();

  Renderer(const Renderer &) = delete;
  Renderer &operator=(const Renderer &) = delete;

  void Init(const RendererCreateInfo &CreateInfo);
  void Shutdown();
  void BeginFrame();
  void Render();
  void EndFrame();
  void SetViewMode(RendererViewMode ViewMode);
  void SetViewportFrameOutput(IViewportFrameOutput *FrameOutput);
  std::optional<CapturedFrame> ConsumeCapturedFrame();
  void SetCpuFrameTime(float CpuFrameMs);
  const RendererFrameStats &GetFrameStats() const;
  std::shared_ptr<Mesh> CreateMesh(const MeshData &MeshData);
  std::vector<RenderMeshSubmission>
  LoadMeshSceneFromFile(
      const std::filesystem::path &Path,
      const MeshSceneLoadOptions &Options = {});

private:
  void UpdateCpuRenderTime(float CpuRenderMs);

private:
  static Renderer *s_Instance;

  std::unique_ptr<RendererBackend> m_Backend;
  std::unique_ptr<RenderTechnique> m_Technique;
  RenderScene m_Scene;
  bool m_IsInitialized{false};
};
} // namespace Axiom
