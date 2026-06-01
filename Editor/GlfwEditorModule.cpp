#include "GlfwEditorModule.h"

#include <Assets/CookedAssetRuntime.h>
#include <Core/Application.h>
#include <Core/Window.h>
#include <Session/StartupScene.h>

#include <filesystem>

#ifndef AXIOM_CONTENT_DIR
#define AXIOM_CONTENT_DIR "Content"
#endif

namespace Axiom {
namespace {
bool LooksLikeContentRoot(const std::filesystem::path &Path) {
  if (Path.empty()) {
    return false;
  }

  std::error_code Error;
  if (!std::filesystem::exists(Path, Error) || Error) {
    return false;
  }

  return std::filesystem::exists(Path / "scene.json") ||
         std::filesystem::exists(Path / "Cooked" / "AssetCookManifest.json") ||
         Assets::IsCookedOnlyContentPath(Path);
}

std::filesystem::path ResolveEditorContentRoot() {
  std::error_code Error;
  const std::filesystem::path CurrentPath = std::filesystem::current_path(Error);
  if (!Error) {
    if (LooksLikeContentRoot(CurrentPath)) {
      return CurrentPath;
    }
    if (LooksLikeContentRoot(CurrentPath / "Content")) {
      return CurrentPath / "Content";
    }
  }

  return std::filesystem::path(AXIOM_CONTENT_DIR);
}
} // namespace

GlfwEditorModule::GlfwEditorModule() : m_Session(m_SessionId) {}

std::string_view GlfwEditorModule::GetName() const {
  return "Editor.GlfwEditor";
}

bool GlfwEditorModule::Initialize(Application &App) {
  m_Session.EnsureViewportState(m_LocalUserId);
  m_Session.SetContentDir(ResolveEditorContentRoot());
  m_Session.SetEngineContentDir(std::filesystem::path(AXIOM_CONTENT_DIR) / "Engine");
  Window *Window = App.GetWindow();
  if (Window != nullptr) {
    m_InputModule.Initialize(*Window, m_MoveSpeed);
  }
  return LoadStartupScene(m_Session);
}

void GlfwEditorModule::Update(const ModuleUpdateContext &Context) {
  switch (Context.Phase) {
  case ModuleUpdatePhase::FrameStart:
    m_InputModule.Tick(m_Session, m_SessionId, m_LocalUserId);
    m_Session.Tick();
    m_SelectionModule.Tick(m_Session, m_SessionId, m_LocalUserId,
                           m_InputModule.GetInputPlatform(),
                           Context.App.GetWindow(), m_LastLeftMouseDown);
    break;
  case ModuleUpdatePhase::Render:
    m_RenderModule.Render(m_Session, m_LocalUserId, m_RendererAdapter);
    break;
  case ModuleUpdatePhase::RenderBegin:
  case ModuleUpdatePhase::ImGuiRender:
  case ModuleUpdatePhase::RenderEnd:
    break;
  }
}

void GlfwEditorModule::Shutdown(Application &App) {
  (void)App;
  m_InputModule.Shutdown();
}
} // namespace Axiom
