#include "Sandbox2D.h"
#include "imgui/imgui.h"

#include "Platform/OpenGL/OpenGLShader.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


Sandbox2D::Sandbox2D()
	: Layer("Sandbox2D"), m_CameraController(16.0f/9.0f) {}

void Sandbox2D::OnAttach() {
	W_PROFILE_FUNCTION();

	m_CheckerboardTexture = Wraith::Texture2D::Create("assets/textures/Checkerboard.png");
}

void Sandbox2D::OnDetach() {
	W_PROFILE_FUNCTION();

}

void Sandbox2D::OnUpdate(Wraith::Timestep ts) {
	W_PROFILE_FUNCTION();

	// Update
	m_CameraController.OnUpdate(ts);

	// Render
	Wraith::Renderer2D::ResetStats();
	Wraith::RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
	Wraith::RenderCommand::Clear();

	Wraith::Renderer2D::BeginScene(m_CameraController.GetCamera());
	Wraith::Renderer2D::DrawQuad({ 0.5f, -0.5f }, { 0.5f, 0.75f }, m_SquareColor);
	Wraith::Renderer2D::EndScene();
}

void Sandbox2D::OnImGuiRender() {
	W_PROFILE_FUNCTION();

	ImGui::Begin("Settings");

	auto stats = Wraith::Renderer2D::GetStats();
	ImGui::Text("Renderer2D Stats:");
	ImGui::Text("Draw Calls: %d", stats.DrawCalls);
	ImGui::Text("Quad Count: %d", stats.QuadCount);
	ImGui::Text("Vertices: %d", stats.GetTotalVertexCount());
	ImGui::Text("Indices: %d", stats.GetTotalIndexCount());
	ImGui::End();
}

void Sandbox2D::OnEvent(Wraith::Event& e) {
	m_CameraController.OnEvent(e);
}