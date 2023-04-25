#include "Player.h"

#include <imgui/imgui.h>
#include <glm/gtc/matrix_transform.hpp>

using namespace Wraith;

Player::Player() {
}

void Player::LoadAssets() {
	m_ShipTexture = Texture2D::Create("assets/textures/Bird.png");
}

void Player::OnUpdate(Timestep ts) {
	m_Time += ts;

	if (Input::IsKeyPressed(W_KEY_SPACE)) {
		m_Velocity.y += m_EnginePower;
		if (m_Velocity.y < 0.0f)
			m_Velocity.y += m_EnginePower * 2.0f;
	}
	else {
		m_Velocity.y -= m_Gravity;
	}

	m_Velocity.y = glm::clamp(m_Velocity.y, -20.0f, 20.0f);
	m_Position += m_Velocity * (float)ts;
}

void Player::OnRender() {
	Renderer2D::DrawRotatedQuad(glm::vec3{ m_Position.x, m_Position.y, 0.5f }, glm::vec2{ 1.2f, 0.9f }, glm::radians(GetRotation() + 90.0f), m_ShipTexture);
}

void Player::OnImGuiRender() {
	ImGui::DragFloat("Engine Power", &m_EnginePower, 0.1f);
	ImGui::DragFloat("Gravity", &m_Gravity, 0.1f);
}

void Player::Reset() {
	m_Position = { -10.0f, 0.0f };
	m_Velocity = { 5.0f, 0.0f };
}