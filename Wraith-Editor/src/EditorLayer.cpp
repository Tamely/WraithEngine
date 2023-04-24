#include "EditorLayer.h"
#include "imgui/imgui.h"

#include "Platform/OpenGL/OpenGLShader.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Wraith {
	EditorLayer::EditorLayer()
		: Layer("Wraith-Editor"), m_CameraController(16.0f / 9.0f) {}

	void EditorLayer::OnAttach() {
		W_PROFILE_FUNCTION();

		m_CheckerboardTexture = Texture2D::Create("assets/textures/Checkerboard.png");

		FramebufferSpecification framebufferSpecification;
		framebufferSpecification.Width = 1280;
		framebufferSpecification.Height = 720;
		m_Framebuffer = Framebuffer::Create(framebufferSpecification);
	}

	void EditorLayer::OnDetach() {
		W_PROFILE_FUNCTION();

	}

	void EditorLayer::OnUpdate(Timestep ts) {
		W_PROFILE_FUNCTION();

		// Update
		m_CameraController.OnUpdate(ts);

		// Render
		Renderer2D::ResetStats();
		m_Framebuffer->Bind();
		RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
		RenderCommand::Clear();

		Renderer2D::BeginScene(m_CameraController.GetCamera());
		Renderer2D::DrawQuad({ 0.5f, -0.5f }, { 0.5f, 0.75f }, m_SquareColor);
		Renderer2D::EndScene();

		m_Framebuffer->Unbind();
	}

	void EditorLayer::OnImGuiRender() {
		W_PROFILE_FUNCTION();

		static bool dockspaceOpen = true;
		static bool opt_fullscreen_persistant = true;
		bool opt_fullscreen = opt_fullscreen_persistant;
		static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

		ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
		if (opt_fullscreen) {
			ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->Pos);
			ImGui::SetNextWindowSize(viewport->Size);
			ImGui::SetNextWindowViewport(viewport->ID);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
			window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
		}

		if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
			window_flags |= ImGuiWindowFlags_NoBackground;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("DockSpace Demo", &dockspaceOpen, window_flags);
		ImGui::PopStyleVar();

		if (opt_fullscreen)
			ImGui::PopStyleVar(2);

		ImGuiIO& io = ImGui::GetIO();
		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
			ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
			ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
		}

		if (ImGui::BeginMenuBar()) {
			if (ImGui::BeginMenu("File")) {
				if (ImGui::MenuItem("Exit")) Application::Get().Close();
				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}


		// Settings
		{
			ImGui::Begin("Settings");

			auto stats = Renderer2D::GetStats();
			ImGui::Text("Renderer2D Stats:");
			ImGui::Text("Draw Calls: %d", stats.DrawCalls);
			ImGui::Text("Quad Count: %d", stats.QuadCount);
			ImGui::Text("Vertices: %d", stats.GetTotalVertexCount());
			ImGui::Text("Indices: %d", stats.GetTotalIndexCount());

			uint32_t textureID = m_Framebuffer->GetColorAttachmentRendererID();
			ImGui::Image((void*)textureID, ImVec2(1280.0f, 720.0f), ImVec2(0, 1), ImVec2(1, 0));

			ImGui::End();
		}

		ImGui::End();
	}

	void EditorLayer::OnEvent(Event& e) {
		m_CameraController.OnEvent(e);
	}
}