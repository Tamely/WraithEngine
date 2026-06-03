#pragma once

#include "Core/ModuleManager.h"
#include "Core/RenderRuntime.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "Core/Window.h"

namespace Axiom {
class Renderer;
struct RendererDeleter {
  void operator()(Renderer *Value) const;
};
using RendererPtr = std::unique_ptr<Renderer, RendererDeleter>;

struct ApplicationArgs {
  char **Arguments;
  int ArgumentCount;
};

enum class RuntimeMode {
  LocalWindowedEditor,
  LocalPackagedGame,
  HeadlessEditorSession
};

struct ApplicationConfig {
  std::string Title{"Axiom Engine"};
  uint32_t Width{1600};
  uint32_t Height{900};
  RuntimeMode Mode{RuntimeMode::LocalWindowedEditor};
  IViewportFrameOutput *FrameOutput{nullptr};
};

class Application {
public:
  struct RuntimeDependencies {
    std::unique_ptr<Window> Window;
    RenderSurfacePtr RenderSurface;
    RendererPtr Renderer;
    bool InitializeRenderer{true};
    bool RegisterDefaultModules{true};
  };

  Application(const ApplicationConfig &Config, const ApplicationArgs &Args);
  virtual ~Application();

  static Application &Get();
  static Application *TryGet();

  void Run();
  bool Step();
  [[nodiscard]] IRenderSurface &GetRenderSurface() const { return *m_RenderSurface; }
  [[nodiscard]] Window *GetWindow() const;
  [[nodiscard]] float GetDeltaTime() const { return m_DeltaTime; }
  [[nodiscard]] uint64_t GetFrameIndex() const { return m_FrameIndex; }
  [[nodiscard]] RuntimeMode GetRuntimeMode() const { return m_Config.Mode; }
  [[nodiscard]] Renderer &GetRenderer() const;
  [[nodiscard]] Renderer *TryGetRenderer() const;
  [[nodiscard]] ModuleManager &GetModuleManager() { return m_ModuleManager; }
  [[nodiscard]] const ModuleManager &GetModuleManager() const {
    return m_ModuleManager;
  }
  void RequestClose();
  void SetRendererViewMode(RendererViewMode ViewMode);
  void SetViewportFrameUser(SessionUserId User);
  void SetViewportFrameOutput(IViewportFrameOutput *FrameOutput);

protected:
  Application(const ApplicationConfig &Config, const ApplicationArgs &Args,
              RuntimeDependencies Dependencies);

  virtual size_t BeginRenderPasses();
  virtual void PrepareRenderPass(size_t PassIndex);
  virtual bool ShouldRenderImGuiForPass(size_t PassIndex,
                                        size_t PassCount) const;
  void RegisterDefaultModules();

private:
  static Application *s_Instance;

  ApplicationConfig m_Config;
  std::unique_ptr<Window> m_Window;
  RenderSurfacePtr m_RenderSurface;
  RendererPtr m_Renderer;
  ModuleManager m_ModuleManager;
  std::chrono::steady_clock::time_point m_LastFrameTime;
  float m_DeltaTime{0.0f};
  uint64_t m_FrameIndex{0};
};

Application *Create(const ApplicationArgs &Args);
} // namespace Axiom
