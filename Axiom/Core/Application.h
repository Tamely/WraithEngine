#pragma once

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

  void Run();
  void PushLayer(Layer *Layer);

private:
  std::string m_Title;
  Window *m_Window;
  Renderer *m_Renderer;
  LayerStack m_LayerStack;
};

Application *Create(const ApplicationArgs &Args);
} // namespace Axiom

