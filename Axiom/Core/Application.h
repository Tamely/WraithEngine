#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include "Core/LayerStack.h"
#include "Core/Window.h"
#include "Renderer/Renderer.h"

namespace Axiom {
struct ApplicationArgs {
  char **Arguments;
  int ArgumentCount;
};

class Application {
public:
  Application(const std::string &Title, const ApplicationArgs &Args);
  virtual ~Application();

  static Application &Get();

  void Run();
  void PushLayer(Layer *Layer);
  [[nodiscard]] Window &GetWindow() const { return *m_Window; }
  [[nodiscard]] float GetDeltaTime() const { return m_DeltaTime; }
  [[nodiscard]] uint64_t GetFrameIndex() const { return m_FrameIndex; }

private:
  static Application *s_Instance;

  std::string m_Title;
  Window *m_Window;
  Renderer *m_Renderer;
  LayerStack m_LayerStack;
  std::chrono::steady_clock::time_point m_LastFrameTime;
  float m_DeltaTime{0.0f};
  uint64_t m_FrameIndex{0};
};

Application *Create(const ApplicationArgs &Args);
} // namespace Axiom
