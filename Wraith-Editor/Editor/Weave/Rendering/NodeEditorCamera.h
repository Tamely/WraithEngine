#pragma once

#include <glm/glm.hpp>

#include <Wraith.h>

namespace Wraith {
	class NodeEditorCamera : public Camera {
	public:
		glm::mat4 GetViewMatrix() const;
		glm::vec2 ScreenToWorld(glm::vec2 screenPos) const;
		glm::vec2 WorldToScreen(glm::vec2 worldPos) const;

	public:
		void SetViewportSize(glm::vec2 size) { m_ViewportSize = size; }
		void Pan(glm::vec2 delta) { m_Position += delta / m_Zoom; }
		void Zoom(float delta) { m_Zoom = glm::clamp(m_Zoom + delta, 0.1f, 5.0f); }

		glm::vec2 GetPosition() const { return m_Position; }
		float GetZoom() const { return m_Zoom; }

	private:
		glm::vec2 m_Position = { 0.0f, 0.0f };
		float m_Zoom = 1.0f;
		glm::vec2 m_ViewportSize;
	};
}
