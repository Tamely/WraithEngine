#include "GameLayer.h"

#include "Random.h"
#include <imgui/imgui.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

GameLayer::GameLayer()
	: Layer("Game"), m_CameraController(16.0f/ 9.0f) {
	CreateCamera(Wraith::Application::Get().GetWindow().GetWidth(), Wraith::Application::Get().GetWindow().GetHeight());
	Random::Init();
}

void GameLayer::OnAttach() {
	m_Level.Init();

	ImGuiIO io = ImGui::GetIO();
	m_Font = io.Fonts->AddFontFromFileTTF("assets/OpenSans-Regular.ttf", 120.0f);
}

void GameLayer::OnDetach() {

}

void GameLayer::OnUpdate(Wraith::Timestep ts) {
	m_Time += ts;
	if ((int)(m_Time * 10.0f) % 8 > 4)
		m_Blink = !m_Blink;

	if (m_Level.IsGameOver())
		m_State = GameState::GameOver;

	const auto& playerPos = m_Level.GetPlayer().GetPosition();
	m_CameraController.GetCamera().SetPosition({ playerPos.x, playerPos.y, 0.0f });

	switch (m_State)
	{
		case GameState::Play:
		{
			m_Level.OnUpdate(ts);
			break;
		}
	}

	// Render
	Wraith::RenderCommand::SetClearColor({ 0.0f, 0.0f, 0.0f, 1 });
	Wraith::RenderCommand::Clear();

	Wraith::Renderer2D::BeginScene(m_CameraController.GetCamera());
	m_Level.OnRender();
	Wraith::Renderer2D::EndScene();
}

void GameLayer::OnImGuiRender() {
	//ImGui::Begin("Settings");
	//m_Level.OnImGuiRender();
	//ImGui::End();

	// UI?

	switch (m_State)
	{
		case GameState::Play:
		{
			uint32_t playerScore = m_Level.GetPlayer().GetScore();
			std::string scoreStr = std::string("Score: ") + std::to_string(playerScore);
			ImGui::GetForegroundDrawList()->AddText(m_Font, 48.0f, ImGui::GetWindowPos(), 0xffffffff, scoreStr.c_str());
			break;
		}
		case GameState::MainMenu:
		{
			auto pos = ImGui::GetWindowPos();
			auto width = Wraith::Application::Get().GetWindow().GetWidth();
			auto height = Wraith::Application::Get().GetWindow().GetHeight();
			pos.x += width * 0.5f - 300.0f;
			pos.y += 50.0f;
			if (m_Blink)
				ImGui::GetForegroundDrawList()->AddText(m_Font, 120.0f, pos, 0xffffffff, "Click to Play!");
			break;
		}
		case GameState::GameOver:
		{
			auto pos = ImGui::GetWindowPos();
			auto width = Wraith::Application::Get().GetWindow().GetWidth();
			auto height = Wraith::Application::Get().GetWindow().GetHeight();
			pos.x += width * 0.5f - 300.0f;
			pos.y += 50.0f;
			if (m_Blink)
				ImGui::GetForegroundDrawList()->AddText(m_Font, 120.0f, pos, 0xffffffff, "Click to Play!");

			pos.x += 200.0f;
			pos.y += 150.0f;
			uint32_t playerScore = m_Level.GetPlayer().GetScore();
			std::string scoreStr = std::string("Score: ") + std::to_string(playerScore);
			ImGui::GetForegroundDrawList()->AddText(m_Font, 48.0f, pos, 0xffffffff, scoreStr.c_str());
			break;
		}
	}
}

void GameLayer::OnEvent(Wraith::Event& e) {
	Wraith::EventDispatcher dispatcher(e);
	dispatcher.Dispatch<Wraith::WindowResizeEvent>(W_BIND_EVENT_FN(GameLayer::OnWindowResize));
	dispatcher.Dispatch<Wraith::MouseButtonPressedEvent>(W_BIND_EVENT_FN(GameLayer::OnMouseButtonPressed));
}

bool GameLayer::OnMouseButtonPressed(Wraith::MouseButtonPressedEvent& e) {
	if (m_State == GameState::GameOver) {
		m_Level.Reset();
	}

	m_State = GameState::Play;
	return false;
}

bool GameLayer::OnWindowResize(Wraith::WindowResizeEvent& e) {
	CreateCamera(e.GetWidth(), e.GetHeight());
	return false;
}

void GameLayer::CreateCamera(uint32_t width, uint32_t height) {
	float aspectRatio = (float)width / (float)height;
	m_CameraController = Wraith::OrthographicCameraController(aspectRatio);
	m_CameraController.SetZoomLevel(8.0f);
	m_CameraController.LockZoomLevel();
}