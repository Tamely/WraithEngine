#include <Core/Application.h>
#include <Core/Entry.h>

#include "GlfwEditorLayer.h"

class EditorApplication : public Axiom::Application {
public:
  EditorApplication(const Axiom::ApplicationArgs &Args)
      : Axiom::Application({.Title = "Axiom Engine",
                            .Width = 1600,
                            .Height = 900,
                            .Mode = Axiom::RuntimeMode::LocalWindowedEditor},
                           Args) {
    PushLayer(new Axiom::GlfwEditorLayer());
  }
};

Axiom::Application *Axiom::Create(const ApplicationArgs &Args) {
  return new EditorApplication(Args);
}
