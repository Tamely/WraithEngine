#include <Core/Application.h>
#include <Core/ApplicationModules.h>
#include <Core/GlfwEditorInputSource.h>
#include <Core/Entry.h>

#include "GlfwEditorModule.h"

class EditorApplication : public Axiom::Application {
public:
  EditorApplication(const Axiom::ApplicationArgs &Args)
      : Axiom::Application({.Title = "Axiom Engine",
                            .Width = 1600,
                            .Height = 900,
                            .Mode = Axiom::RuntimeMode::LocalWindowedEditor},
                           Args,
                           {.RegisterDefaultModules = false}) {
    GetModuleManager().RegisterModule(std::make_unique<Axiom::WindowEventsModule>());
    GetModuleManager().RegisterModule(std::make_unique<Axiom::GlfwEditorModule>());
    GetModuleManager().RegisterModule(std::make_unique<Axiom::RendererFrameModule>());
  }
};

Axiom::Application *Axiom::Create(const ApplicationArgs &Args) {
  return new EditorApplication(Args);
}
