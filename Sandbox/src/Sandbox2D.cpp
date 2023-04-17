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
	Wraith::RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
	Wraith::RenderCommand::Clear();

	static float rotation = 0.0f;
	rotation += ts * 50.f;

	Wraith::Renderer2D::BeginScene(m_CameraController.GetCamera());
	Wraith::Renderer2D::DrawRotatedQuad({ 1.0f, 0.0f }, { 0.8f, 0.8f }, -45.0f, { 0.8f, 0.2f, 0.3f, 1.0f });
	Wraith::Renderer2D::DrawQuad({ -1.0f, 0.0f }, { 0.8f, 0.8f }, { 0.8f, 0.2f, 0.3f, 1.0f });
	Wraith::Renderer2D::DrawQuad({ 0.5f, -0.5f }, { 0.5f, 0.75f }, { 0.2f, 0.3f, 0.8f, 1.0f });
	Wraith::Renderer2D::DrawQuad({ 0.0f, 0.0f, -0.1f }, { 10.0f, 10.0f }, m_CheckerboardTexture, 10.0f);
	Wraith::Renderer2D::DrawRotatedQuad({ 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f }, rotation, m_CheckerboardTexture, 20.0f);
	Wraith::Renderer2D::EndScene();
}

void Sandbox2D::OnImGuiRender() {
	W_PROFILE_FUNCTION();
}

void Sandbox2D::OnEvent(Wraith::Event& e) {
	m_CameraController.OnEvent(e);
}