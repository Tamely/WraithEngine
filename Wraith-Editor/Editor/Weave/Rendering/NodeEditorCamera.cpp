#include "wpch.h"
#include "NodeEditorCamera.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Wraith {
	glm::mat4 NodeEditorCamera::GetViewMatrix() const {
		glm::mat4 view = glm::mat4(1.0f);
		view = glm::translate(view, glm::vec3(-m_Position * m_Zoom, 0.0f));
		view = glm::scale(view, glm::vec3(m_Zoom, m_Zoom, 1.0f));

		return view;
	}

	glm::vec2 NodeEditorCamera::ScreenToWorld(glm::vec2 screenPos) const {
		glm::vec2 centered = screenPos - (m_ViewportSize * 0.5f);
		glm::vec2 worldPos = (centered / m_Zoom) + m_Position;

		return worldPos;
	}

	glm::vec2 NodeEditorCamera::WorldToScreen(glm::vec2 worldPos) const {
		glm::vec2 relative = (worldPos - m_Position) * m_Zoom;
		glm::vec2 screenPos = relative + (m_ViewportSize * 0.5f);

		return screenPos;
	}
}
