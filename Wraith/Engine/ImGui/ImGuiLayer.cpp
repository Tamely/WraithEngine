#include "wpch.h"
#include "ImGuiLayer.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include "Core/Application.h"

// TEMPORARY
#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include "ImGuizmo.h"

namespace Wraith {

	ImGuiLayer::ImGuiLayer()
		: Layer("ImGuiLayer") {
	}

	ImGuiLayer::~ImGuiLayer() {}

	void ImGuiLayer::OnAttach() {
		W_PROFILE_FUNCTION();

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard controls
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Enable Docking
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport

		io.Fonts->AddFontFromFileTTF("Content/Fonts/Roboto/Static/Roboto-Bold.ttf", 16.0f);
		io.FontDefault = io.Fonts->AddFontFromFileTTF("Content/Fonts/Roboto/Roboto-VariableFont_wdth,wght.ttf", 16.0f);

		ImGui::StyleColorsDark();

		ImGuiStyle& style = ImGui::GetStyle();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		SetDarkTheme();

		Application& app = Application::Get();
		GLFWwindow* window = static_cast<GLFWwindow*>(app.GetWindow().GetNativeWindow());

		ImGui_ImplGlfw_InitForOpenGL(window, true);
		ImGui_ImplOpenGL3_Init("#version 410");
	}

	void ImGuiLayer::OnDetach() {
		W_PROFILE_FUNCTION();

		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}

	void ImGuiLayer::Begin() {
		W_PROFILE_FUNCTION();

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		ImGuizmo::BeginFrame();
	}

	void ImGuiLayer::End() {
		W_PROFILE_FUNCTION();

		ImGuiIO& io = ImGui::GetIO();
		Application& app = Application::Get();
		io.DisplaySize = ImVec2((float)app.GetWindow().GetWidth(), (float)app.GetWindow().GetHeight());

		// Rendering
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
			GLFWwindow* backup_current_context = glfwGetCurrentContext();
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			glfwMakeContextCurrent(backup_current_context);
		}
	}

	void ImGuiLayer::SetDarkTheme() {
		ImGuiStyle& style = ImGui::GetStyle();

		style.Alpha = 1.0f;
		style.DisabledAlpha = 1.0f;
		style.WindowPadding = ImVec2(12.0f, 12.0f);
		style.WindowRounding = 11.5f;
		style.WindowBorderSize = 0.0f;
		style.WindowMinSize = ImVec2(20.0f, 20.0f);
		style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
		style.WindowMenuButtonPosition = ImGuiDir_Right;
		style.ChildRounding = 0.0f;
		style.ChildBorderSize = 1.0f;
		style.PopupRounding = 0.0f;
		style.PopupBorderSize = 1.0f;
		style.FramePadding = ImVec2(20.0f, 3.400000095367432f);
		style.FrameRounding = 11.89999961853027f;
		style.FrameBorderSize = 0.0f;
		style.ItemSpacing = ImVec2(4.300000190734863f, 5.5f);
		style.ItemInnerSpacing = ImVec2(7.099999904632568f, 1.799999952316284f);
		style.CellPadding = ImVec2(12.10000038146973f, 9.199999809265137f);
		style.IndentSpacing = 0.0f;
		style.ColumnsMinSpacing = 4.900000095367432f;
		style.ScrollbarSize = 11.60000038146973f;
		style.ScrollbarRounding = 15.89999961853027f;
		style.GrabMinSize = 3.700000047683716f;
		style.GrabRounding = 20.0f;
		style.TabRounding = 0.0f;
		style.TabBorderSize = 0.0f;
		style.TabMinWidthForCloseButton = 0.0f;
		style.ColorButtonPosition = ImGuiDir_Right;
		style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
		style.SelectableTextAlign = ImVec2(0.0f, 0.0f);

		auto& colors = style.Colors;

		// Main background
		colors[ImGuiCol_WindowBg] = ImVec4{ 0.06f, 0.06f, 0.06f, 1.0f };

		// Headers
		colors[ImGuiCol_Header] = ImVec4{ 0.12f, 0.12f, 0.14f, 1.0f };
		colors[ImGuiCol_HeaderHovered] = ImVec4{ 0.20f, 0.22f, 0.27f, 1.0f };
		colors[ImGuiCol_HeaderActive] = ImVec4{ 0.16f, 0.18f, 0.22f, 1.0f };

		// Buttons
		colors[ImGuiCol_Button] = ImVec4{ 0.12f, 0.12f, 0.14f, 1.0f };
		colors[ImGuiCol_ButtonHovered] = ImVec4{ 0.20f, 0.22f, 0.27f, 1.0f };
		colors[ImGuiCol_ButtonActive] = ImVec4{ 0.28f, 0.56f, 1.0f, 1.0f };  // Blue accent

		// Frame BG - Input fields, etc.
		colors[ImGuiCol_FrameBg] = ImVec4{ 0.10f, 0.10f, 0.12f, 1.0f };
		colors[ImGuiCol_FrameBgHovered] = ImVec4{ 0.16f, 0.16f, 0.18f, 1.0f };
		colors[ImGuiCol_FrameBgActive] = ImVec4{ 0.18f, 0.18f, 0.20f, 1.0f };

		// Tabs
		colors[ImGuiCol_Tab] = ImVec4{ 0.08f, 0.08f, 0.10f, 1.0f };
		colors[ImGuiCol_TabHovered] = ImVec4{ 0.20f, 0.22f, 0.27f, 1.0f };
		colors[ImGuiCol_TabActive] = ImVec4{ 0.14f, 0.14f, 0.16f, 1.0f };
		colors[ImGuiCol_TabUnfocused] = ImVec4{ 0.08f, 0.08f, 0.10f, 1.0f };
		colors[ImGuiCol_TabUnfocusedActive] = ImVec4{ 0.12f, 0.12f, 0.14f, 1.0f };

		// Title bars
		colors[ImGuiCol_TitleBg] = ImVec4{ 0.08f, 0.08f, 0.10f, 1.0f };
		colors[ImGuiCol_TitleBgActive] = ImVec4{ 0.12f, 0.12f, 0.14f, 1.0f };
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4{ 0.08f, 0.08f, 0.10f, 1.0f };

		// Misc
		colors[ImGuiCol_MenuBarBg] = ImVec4{ 0.08f, 0.08f, 0.10f, 1.0f };
		colors[ImGuiCol_ScrollbarBg] = ImVec4{ 0.06f, 0.06f, 0.06f, 1.0f };
		colors[ImGuiCol_ScrollbarGrab] = ImVec4{ 0.16f, 0.16f, 0.18f, 1.0f };
		colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4{ 0.20f, 0.22f, 0.27f, 1.0f };
		colors[ImGuiCol_ScrollbarGrabActive] = ImVec4{ 0.28f, 0.56f, 1.0f, 1.0f };
		colors[ImGuiCol_CheckMark] = ImVec4{ 0.28f, 0.56f, 1.0f, 1.0f };
		colors[ImGuiCol_SliderGrab] = ImVec4{ 0.28f, 0.56f, 1.0f, 1.0f };
		colors[ImGuiCol_SliderGrabActive] = ImVec4{ 0.35f, 0.61f, 1.0f, 1.0f };
		colors[ImGuiCol_Separator] = ImVec4{ 0.16f, 0.16f, 0.18f, 1.0f };
		colors[ImGuiCol_SeparatorHovered] = ImVec4{ 0.28f, 0.56f, 1.0f, 1.0f };
		colors[ImGuiCol_SeparatorActive] = ImVec4{ 0.35f, 0.61f, 1.0f, 1.0f };
		colors[ImGuiCol_ResizeGrip] = ImVec4{ 0.16f, 0.16f, 0.18f, 1.0f };
		colors[ImGuiCol_ResizeGripHovered] = ImVec4{ 0.28f, 0.56f, 1.0f, 1.0f };
		colors[ImGuiCol_ResizeGripActive] = ImVec4{ 0.35f, 0.61f, 1.0f, 1.0f };
	}

	void ImGuiLayer::OnEvent(Event& e) {
		if (m_BlockEvents) {
			ImGuiIO& io = ImGui::GetIO();
			e.Handled |= e.IsInCategory(EventCategoryMouse) & io.WantCaptureMouse;
			e.Handled |= e.IsInCategory(EventCategoryKeyboard) & io.WantCaptureKeyboard;
		}
	}
}
