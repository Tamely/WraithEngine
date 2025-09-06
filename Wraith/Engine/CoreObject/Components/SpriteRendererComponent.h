#pragma once
#include "ComponentMacros.h"
#include <glm/glm.hpp>
#include "Rendering/Texture.h"

namespace Wraith {
	struct SpriteRendererComponent {
		GENERATED_COMPONENT_BODY(SpriteRendererComponent);

		glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
		Ref<Texture2D> Texture;
		float TilingFactor = 1.0f;

		SpriteRendererComponent() = default;
		SpriteRendererComponent(const SpriteRendererComponent&) = default;
		SpriteRendererComponent(const glm::vec4& color) : Color(color) {}
	};
}
