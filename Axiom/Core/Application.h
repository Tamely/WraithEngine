#pragma once

#include "Renderer/RenderSurface.h"
#include "Renderer/ViewportFrameOutput.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

#include "Core/LayerStack.h"
#include "Core/Window.h"
#include "Renderer/Renderer.h"

namespace Axiom {
struct ApplicationArgs {
  char **Arguments;
  int ArgumentCount;
};

struct ApplicationCreateInfo {
  std::shared_ptr<IRenderSurface> Surface;
  IViewportFrameOutput *FrameOutput{nullptr};
};

class Application {
public:
  Application(const std::string &Title, const ApplicationArgs &Args,
              ApplicationCreateInfo CreateInfo = {});
  virtual ~Application();

  static Application &Get();

  void Run();
  void PushLayer(Layer *Layer);
  [[nodiscard]] IRenderSurface &GetRenderSurface() const { return *m_Surface; }
  [[nodiscard]] Window *GetWindow() const;
  [[nodiscard]] float GetDeltaTime() const { return m_DeltaTime; }
  [[nodiscard]] uint64_t GetFrameIndex() const { return m_FrameIndex; }

private:
  static Application *s_Instance;

  std::string m_Title;
  std::shared_ptr<IRenderSurface> m_Surface;
  Renderer *m_Renderer;
  LayerStack m_LayerStack;
  std::chrono::steady_clock::time_point m_LastFrameTime;
  float m_DeltaTime{0.0f};
  uint64_t m_FrameIndex{0};
};

Application *Create(const ApplicationArgs &Args);
} // namespace Axiom
