#pragma once

#include "Core/Pin.h"

#include <glm/glm.hpp>

#include <any>
#include <string>
#include <vector>

namespace Wraith {
	struct Node {
		uint32_t ID;
		std::string Title;
		glm::vec2 Position;
		glm::vec2 Size = { 200.0f, 100.0f };
		std::vector<Pin> InputPins;
		std::vector<Pin> OutputPins;
		bool IsSelected = false;

		// Node-specific data
		glm::vec4 Color = { 0.3f, 0.3f, 0.3f, 1.0f };
		std::string NodeType; // "Add", "Multiply", "GetEntityLocation", etc
		std::any NodeData; // Custom data for different node types

		void UpdatePinPositions();

		Pin* GetPin(uint32_t pinID);
		const Pin* GetPin(uint32_t pinID) const;
	};
}
