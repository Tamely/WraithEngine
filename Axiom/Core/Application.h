#pragma once

#include "Renderer/RenderSurface.h"
#include "Renderer/ViewportFrameOutput.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <cstddef>
#include <string>

#include "Core/LayerStack.h"
#include "Core/Window.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderSurface.h"

namespace Axiom {
struct ApplicationArgs {
  char **Arguments;
  int ArgumentCount;
};

enum class RuntimeMode { LocalWindowedEditor, HeadlessEditorSession };

struct ApplicationConfig {
  std::string Title{"Axiom Engine"};
  uint32_t Width{1600};
  uint32_t Height{900};
  RuntimeMode Mode{RuntimeMode::LocalWindowedEditor};
  IViewportFrameOutput *FrameOutput{nullptr};
};

class Application {
public:
  Application(const ApplicationConfig &Config, const ApplicationArgs &Args);
  virtual ~Application();

  static Application &Get();

  void Run();
  bool Step();
  void PushLayer(Layer *Layer);
  [[nodiscard]] IRenderSurface &GetRenderSurface() const {
    return *m_RenderSurface;
  }
  [[nodiscard]] Window *GetWindow() const;
  [[nodiscard]] float GetDeltaTime() const { return m_DeltaTime; }
  [[nodiscard]] uint64_t GetFrameIndex() const { return m_FrameIndex; }
  [[nodiscard]] RuntimeMode GetRuntimeMode() const { return m_Config.Mode; }
  [[nodiscard]] Renderer &GetRenderer() const { return *m_Renderer; }
  void RequestClose();
  void SetRendererViewMode(RendererViewMode ViewMode);
  void SetViewportFrameOutput(IViewportFrameOutput *FrameOutput);

protected:
  virtual size_t BeginRenderPasses();
  virtual void PrepareRenderPass(size_t PassIndex);
  virtual bool ShouldRenderImGuiForPass(size_t PassIndex,
                                        size_t PassCount) const;

private:
  static Application *s_Instance;

  ApplicationConfig m_Config;
  std::unique_ptr<Window> m_Window;
  RenderSurfacePtr m_RenderSurface;
  std::unique_ptr<Renderer> m_Renderer;
  LayerStack m_LayerStack;
  std::chrono::steady_clock::time_point m_LastFrameTime;
  float m_DeltaTime{0.0f};
  uint64_t m_FrameIndex{0};
};

Application *Create(const ApplicationArgs &Args);
} // namespace Axiom
