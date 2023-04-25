#pragma once

#include <Wraith.h>

#include "Level.h"
#include <imgui/imgui.h>

enum class GameState {
	GameOver,
	MainMenu,
	Play
};

class GameLayer : public Wraith::Layer {
public:
	GameLayer();
	virtual ~GameLayer() = default;

	virtual void OnAttach() override;
	virtual void OnDetach() override;

	virtual void OnUpdate(Wraith::Timestep ts) override;
	virtual void OnImGuiRender() override;
	virtual void OnEvent(Wraith::Event& e) override;
	bool OnMouseButtonPressed(Wraith::MouseButtonPressedEvent& e);
	bool OnWindowResize(Wraith::WindowResizeEvent& e);
private:
	void CreateCamera(uint32_t width, uint32_t height);
private:
	Wraith::OrthographicCameraController m_CameraController;

	Level m_Level;
	ImFont* m_Font;
	float m_Time = 0.0f;
	bool m_Blink = false;

	GameState m_State = GameState::MainMenu;
};