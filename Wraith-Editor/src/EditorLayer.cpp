#include "EditorLayer.h"
#include <imgui.h>

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
		framebufferSpecification.Width = 1600;
		framebufferSpecification.Height = 900;
		m_Framebuffer = Framebuffer::Create(framebufferSpecification);

		m_ActiveScene = CreateRef<Scene>();
		m_SquareEntity = m_ActiveScene->CreateEntity("Square A");
		m_SquareEntity.AddComponent<SpriteRendererComponent>(glm::vec4{ 0.0f, 1.0f, 0.0f, 1.0f });
		m_SquareEntity.AddComponent<TextureComponent>(m_CheckerboardTexture);

		auto redSquare = m_ActiveScene->CreateEntity("Square B");
		redSquare.AddComponent<SpriteRendererComponent>(glm::vec4{ 1.0f, 0.0f, 0.0f, 1.0f });

		m_CameraEntity = m_ActiveScene->CreateEntity("Camera A");
		m_CameraEntity.AddComponent<CameraComponent>();

		m_SecondCamera = m_ActiveScene->CreateEntity("Camera B");
		auto& secondCameraComponent = m_SecondCamera.AddComponent<CameraComponent>();
		secondCameraComponent.Primary = false;

		class CameraController : public ScriptableEntity {
		public:
			void OnCreate() {
				// This will just spawn the cameras at a random position so I can test "Make Primary" easier
				auto& translation = GetComponent<TransformComponent>().Translation;
				translation.x = rand() % 10 - 5.0f;
				translation.y = rand() % 10 - 5.0f;
			}

			void OnDestroy() {

			}

			void OnUpdate(Timestep ts) {
				if (!GetComponent<CameraComponent>().Primary) return;

				auto& translation = GetComponent<TransformComponent>().Translation;
				float speed = 5.0f;

				if (Input::IsKeyPressed(W_KEY_A)) {
					translation.x -= speed * ts;
				}
				if (Input::IsKeyPressed(W_KEY_D)) {
					translation.x += speed * ts;
				}
				if (Input::IsKeyPressed(W_KEY_W)) {
					translation.y += speed * ts;
				}
				if (Input::IsKeyPressed(W_KEY_S)) {
					translation.y -= speed * ts;
				}
			}
		};

		m_CameraEntity.BindNativeScript<CameraController>();
		m_SecondCamera.BindNativeScript<CameraController>();

		m_SceneHierarchyPanel.SetContext(m_ActiveScene);
	}

	void EditorLayer::OnDetach() {
		W_PROFILE_FUNCTION();

	}

	void EditorLayer::OnUpdate(Timestep ts) {
		W_PROFILE_FUNCTION();

		// Resize
		if (FramebufferSpecification spec = m_Framebuffer->GetSpecification();
			m_ViewportSize.x > 0.0f && m_ViewportSize.y > 0.0f && // zero sized framebuffer is invalid
			(spec.Width != m_ViewportSize.x || spec.Height != m_ViewportSize.y))
		{
			m_Framebuffer->Resize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
			m_CameraController.OnResize(m_ViewportSize.x, m_ViewportSize.y);

			m_ActiveScene->OnViewportResize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
		}

		// Update
		if (m_ViewportFocused) m_CameraController.OnUpdate(ts);

		// Render
		Renderer2D::ResetStats();
		m_Framebuffer->Bind();
		RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
		RenderCommand::Clear();

		// Update Scene
		m_ActiveScene->OnUpdate(ts);

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

		// DockSpace
		ImGuiIO& io = ImGui::GetIO();
		ImGuiStyle& style = ImGui::GetStyle();
		float minWinSizeX = style.WindowMinSize.x;
		style.WindowMinSize.x = 370.0f;
		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
			ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
			ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
		}

		style.WindowMinSize.x = minWinSizeX;

		if (ImGui::BeginMenuBar()) {
			if (ImGui::BeginMenu("File")) {
				if (ImGui::MenuItem("Exit")) Application::Get().Close();
				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}

		m_SceneHierarchyPanel.OnImGuiRender();

		// Render Stats
		{
			ImGui::Begin("Render Stats");

			auto stats = Renderer2D::GetStats();
			ImGui::Text("Renderer2D Stats:");
			ImGui::Text("Draw Calls: %d", stats.DrawCalls);
			ImGui::Text("Quad Count: %d", stats.QuadCount);
			ImGui::Text("Vertices: %d", stats.GetTotalVertexCount());
			ImGui::Text("Indices: %d", stats.GetTotalIndexCount());

			ImGui::End();
		}

		// Viewport
		{
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
			ImGui::Begin("Viewport");
			
			m_ViewportFocused = ImGui::IsWindowFocused();
			m_ViewportHovered = ImGui::IsWindowHovered();
			Application::Get().GetImGuiLayer()->BlockEvents(!m_ViewportFocused || !m_ViewportHovered);

			ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
			m_ViewportSize = { viewportPanelSize.x, viewportPanelSize.y };

			uint32_t textureID = m_Framebuffer->GetColorAttachmentRendererID();
			ImGui::Image((void*)textureID, ImVec2{ m_ViewportSize.x, m_ViewportSize.y }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
			ImGui::End();
			ImGui::PopStyleVar();
		}

		ImGui::End();
	}

	void EditorLayer::OnEvent(Event& e) {
		m_CameraController.OnEvent(e);
	}
}