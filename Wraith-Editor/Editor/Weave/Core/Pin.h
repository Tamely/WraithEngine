#pragma once

#include <glm/glm.hpp>

#include <any>
#include <string>
#include <vector>

namespace Wraith {
	enum class PinType {
		Float, Vec2, Vec3, Vec4, Int, Bool, String, Exec
	};

	struct Pin {
		uint32_t ID;
		std::string Name;
		PinType Type;
		bool IsInput;
		glm::vec2 Position; // Screen position for rendering
		glm::vec2 WorldPosition;
		std::any Value; // Default value for input pins
		std::vector<uint32_t> ConnectedPins; // IDs of connected pins
		float Radius = 8.0f;
	};
}
