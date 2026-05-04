#include <Core/Application.h>
#include <Core/Entry.h>
#include <Core/Layer.h>
#include <Core/Log.h>

#include <Renderer/Camera.h>
#include <Renderer/RenderCommand.h>
#include <Renderer/Renderer.h>

#include <glm/ext/matrix_transform.hpp>

#include <filesystem>

#ifndef AXIOM_CONTENT_DIR
#define AXIOM_CONTENT_DIR "Content"
#endif

class StartupSceneLayer final : public Axiom::Layer {
public:
  StartupSceneLayer() : Axiom::Layer("StartupSceneLayer") {}

  void OnAttach() override {
    m_Camera.LookAt(glm::vec3(0.0f, 0.8f, 3.5f), glm::vec3(0.0f, 0.3f, 0.0f));
    m_Camera.SetPerspective(55.0f, 1600.0f / 900.0f, 0.1f, 100.0f);
    const auto MeshPath =
        std::filesystem::path(AXIOM_CONTENT_DIR) / "basicmesh.glb";
    m_Mesh = Axiom::Renderer::Get().LoadMeshFromFile(MeshPath);
    if (!m_Mesh) {
      A_CORE_ERROR("Failed to create startup mesh from {0}",
                   MeshPath.string());
    }
  }

  void OnUpdate() override {
    Axiom::RenderCommand::SetCamera(m_Camera);
    if (m_Mesh) {
      Axiom::RenderCommand::Submit({m_Mesh, glm::mat4(1.0f)});
    }
  }

private:
  Axiom::Camera m_Camera;
  Axiom::MeshRef m_Mesh;
};

class EditorApplication : public Axiom::Application {
public:
  EditorApplication(const Axiom::ApplicationArgs &Args)
      : Axiom::Application("Axiom Engine", Args) {
    PushLayer(new StartupSceneLayer());
  }
};

Axiom::Application *Axiom::Create(const ApplicationArgs &Args) {
  return new EditorApplication(Args);
}
