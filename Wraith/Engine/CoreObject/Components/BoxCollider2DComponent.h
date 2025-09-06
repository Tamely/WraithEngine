#pragma once
#include "ComponentMacros.h"
#include <glm/glm.hpp>

namespace Wraith {
	struct BoxCollider2DComponent {
		GENERATED_COMPONENT_BODY(BoxCollider2DComponent);

		glm::vec2 Offset = { 0.0f, 0.0f };
		glm::vec2 Size = { 0.5f, 0.5f };

		// Physics material properties
		float Density = 1.0f;
		float Friction = 0.0f;
		float Restitution = 0.0f;
		float RestitutionThreshold = 0.5f;

		void* RuntimeFixture = nullptr; // Storage for runtime

		BoxCollider2DComponent() = default;
		BoxCollider2DComponent(const BoxCollider2DComponent&) = default;
	};
}
