#include <Core/Application.h>
#include <Core/Entry.h>

class EditorApplication : public Axiom::Application {
public:
  EditorApplication(const Axiom::ApplicationArgs &Args)
      : Axiom::Application("Axiom Engine", Args) {}
};

Axiom::Application *Axiom::Create(const Axiom::ApplicationArgs &Args) {
  return new EditorApplication(Args);
}

