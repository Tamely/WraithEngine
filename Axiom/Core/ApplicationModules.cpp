#include "Core/ApplicationModules.h"

#include "Core/Application.h"

namespace Axiom {
std::string_view WindowEventsModule::GetName() const {
  return "Core.WindowEvents";
}

bool WindowEventsModule::Initialize(Application &App) {
  (void)App;
  return true;
}

void WindowEventsModule::Update(const ModuleUpdateContext &Context) {
  if (Context.Phase != ModuleUpdatePhase::FrameStart) {
    return;
  }

  if (Window *Window = Context.App.GetWindow(); Window != nullptr) {
    Window->PollEvents();
  }
}

void WindowEventsModule::Shutdown(Application &App) { (void)App; }
} // namespace Axiom
