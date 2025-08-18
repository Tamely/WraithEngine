#include "wpch.h"
#include "EditorCamera.h"

#include "Wraith/Core/Input.h"
#include "Wraith/Core/KeyCodes.h"
#include "Wraith/Core/MouseButtonCodes.h"

#include <glfw/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace Wraith {
	EditorCamera::EditorCamera(float fov, float aspectRatio, float nearClip, float farClip)
		: m_FOV(fov), m_AspectRatio(aspectRatio), m_NearClip(nearClip), m_FarClip(farClip) {
		UpdateView();
	}

	void EditorCamera::OnUpdate(Timestep ts) {
		const glm::vec2 mouse{ Input::GetMouseX(), Input::GetMouseY() };

		if (Input::IsMouseButtonPressed(W_MOUSE_BUTTON_RIGHT)) {
			// Mouse rotation
			if (!m_RightMousePressedLastFrame)
				m_InitialMousePosition = mouse;

			glm::vec2 delta = (mouse - m_InitialMousePosition) * 0.003f;
			m_InitialMousePosition = mouse;
			MouseRotate(delta);
			m_RightMousePressedLastFrame = true;

			// WASD movement
			auto [xSpeed, ySpeed] = PanSpeed();
			float moveSpeed = (xSpeed * m_Distance) / 10.0f; // scale by distance like mouse pan
			if (Input::IsKeyPressed(W_KEY_W))
				m_FocalPoint += GetForwardDirection() * moveSpeed;
			if (Input::IsKeyPressed(W_KEY_S))
				m_FocalPoint -= GetForwardDirection() * moveSpeed;
			if (Input::IsKeyPressed(W_KEY_A))
				m_FocalPoint -= GetRightDirection() * moveSpeed;
			if (Input::IsKeyPressed(W_KEY_D))
				m_FocalPoint += GetRightDirection() * moveSpeed;

			// Vertical
			float verticalSpeed = (ySpeed * m_Distance) / 20.0f;
			if (Input::IsKeyPressed(W_KEY_LEFT_SHIFT))
				m_FocalPoint -= GetUpDirection() * verticalSpeed;
			if (Input::IsKeyPressed(W_KEY_SPACE))
				m_FocalPoint += GetUpDirection() * verticalSpeed;
		}
		else {
			m_RightMousePressedLastFrame = false;
		}


		if (Input::IsMouseButtonPressed(W_MOUSE_BUTTON_MIDDLE)) {
			if (!m_MiddleMousePressedLastFrame) {
				m_InitialMousePosition = mouse;
			}
			glm::vec2 delta = (mouse - m_InitialMousePosition) * 0.003f;
			m_InitialMousePosition = mouse;
			MousePan(delta);
			m_MiddleMousePressedLastFrame = true;
		}
		else {
			m_MiddleMousePressedLastFrame = false;
		}

		UpdateView();
	}


	void EditorCamera::OnEvent(Event& e) {
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<MouseScrolledEvent>(W_BIND_EVENT_FN(EditorCamera::OnMouseScroll));
	}

	glm::vec3 EditorCamera::GetUpDirection() const {
		return glm::rotate(GetOrientation(), glm::vec3(0.0f, 1.0f, 0.0f));
	}

	glm::vec3 EditorCamera::GetRightDirection() const {
		return glm::rotate(GetOrientation(), glm::vec3(1.0f, 0.0f, 0.0f));
	}

	glm::vec3 EditorCamera::GetForwardDirection() const {
		return glm::rotate(GetOrientation(), glm::vec3(0.0f, 0.0f, -1.0f));
	}

	glm::quat EditorCamera::GetOrientation() const {
		return glm::quat(glm::vec3(-m_Pitch, -m_Yaw, 0.0f));
	}

	void EditorCamera::UpdateProjection() {
		m_AspectRatio = m_ViewportWidth / m_ViewportHeight;
		m_Projection = glm::perspective(glm::radians(m_FOV), m_AspectRatio, m_NearClip, m_FarClip);
	}

	void EditorCamera::UpdateView() {
		m_Position = CalculatePosition();

		glm::quat orientation = GetOrientation();
		m_ViewMatrix = glm::translate(glm::mat4(1.0f), m_Position) * glm::toMat4(orientation);
		m_ViewMatrix = glm::inverse(m_ViewMatrix);
	}

	bool EditorCamera::OnMouseScroll(MouseScrolledEvent& e) {
		float delta = e.GetYOffset() * 0.1f;
		MouseZoom(delta);
		UpdateView();
		return false;
	}

	void EditorCamera::MousePan(const glm::vec2& delta) {
		auto [xSpeed, ySpeed] = PanSpeed();
		m_FocalPoint += -GetRightDirection() * delta.x * xSpeed * m_Distance;
		m_FocalPoint += GetUpDirection() * delta.y * ySpeed * m_Distance;
	}

	void EditorCamera::MouseRotate(const glm::vec2& delta) {
		float yawSign = GetUpDirection().y < 0 ? -1.0f : 1.0f;
		m_Yaw += yawSign * delta.x * RotationSpeed();
		m_Pitch += delta.y * RotationSpeed();
	}

	void EditorCamera::MouseZoom(float delta) {
		m_Distance -= delta * ZoomSpeed();
		if (m_Distance < 1.0f) {
			m_FocalPoint += GetForwardDirection();
			m_Distance = 1.0f;
		}
	}

	glm::vec3 EditorCamera::CalculatePosition() const {
		return m_FocalPoint - GetForwardDirection() * m_Distance;
	}

	std::pair<float, float> EditorCamera::PanSpeed() const {
		float x = std::min(m_ViewportWidth / 1000.0f, 2.4f); // Max x and y factor is 2.4
		float xFactor = 0.0366f * (x * x) - 0.1778f * x + 0.3021f;

		float y = std::min(m_ViewportHeight / 1000.0f, 2.4f);
		float yFactor = 0.0366f * (y * y) - 0.1778f * y + 0.3021f;

		return { xFactor, yFactor };
	}

	float EditorCamera::RotationSpeed() const {
		return 0.8f;
	}

	float EditorCamera::ZoomSpeed() const {
		float distance = m_Distance * 0.2f;
		distance = std::max(distance, 0.0f);

		float speed = distance * distance;
		speed = std::min(speed, 100.0f); // Sets max speed to 100

		return speed;
	}
}
