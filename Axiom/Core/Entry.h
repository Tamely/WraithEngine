#pragma once

#include "Core/Application.h"

extern Axiom::Application *Axiom::Create(const Axiom::ApplicationArgs &Args);

int main(int argc, char **argv) {
  auto App = Axiom::Create({argv, argc});
  App->Run();
  delete App;
  return 0;
}

