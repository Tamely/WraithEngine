#include "wpch.h"
#include "Node.h"

namespace Wraith {
	void Node::UpdatePinPositions() {
		const float pinSpacing = 25.0f;
		const float headerHeight = 30.0f;
		const float pinOffsetX = 10.0f;

		// Update input pins (right side)
		for (size_t i = 0; i < InputPins.size(); ++i) {
			InputPins[i].Position = { pinOffsetX, headerHeight + i * pinSpacing };
			InputPins[i].WorldPosition = Position + InputPins[i].Position;
		}

		// Update output pins (right side)
		for (size_t i = 0; i < OutputPins.size(); ++i) {
			OutputPins[i].Position = { Size.x - pinOffsetX, headerHeight + i * pinSpacing };
			OutputPins[i].WorldPosition = Position + OutputPins[i].Position;
		}

		// Adjust node height based on pin count
		size_t maxPins = std::max(InputPins.size(), OutputPins.size());
		float minHeight = headerHeight + (maxPins * pinSpacing) + 10.0f;
		if (Size.y < minHeight) {
			Size.y = minHeight;
		}
	}

	Pin* Node::GetPin(uint32_t pinID) {
		for (auto& pin : InputPins) {
			if (pin.ID == pinID) return &pin;
		}

		for (auto& pin : OutputPins) {
			if (pin.ID == pinID) return &pin;
		}

		return nullptr;
	}

	const Pin* Node::GetPin(uint32_t pinID) const {
		for (auto& pin : InputPins) {
			if (pin.ID == pinID) return &pin;
		}

		for (auto& pin : OutputPins) {
			if (pin.ID == pinID) return &pin;
		}

		return nullptr;
	}
}
