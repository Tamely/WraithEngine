#pragma once

#include <glm/glm.hpp>
#include <vector>

namespace Wraith {
	struct Connection {
		uint32_t ID;
		uint32_t FromPinID;
		uint32_t ToPinID;
		std::vector<glm::vec2> BezierPoints; // For curved connections
		glm::vec4 Color = { 1.0f, 1.0f, 1.0f, 1.0f };
	};
}
