#pragma once
#include "ComponentMacros.h"
#include <glm/glm.hpp>
#include "Rendering/Texture.h"

namespace Wraith {
	struct Shape2DComponent {
		GENERATED_COMPONENT_BODY(Shape2DComponent);

		struct QuadSpec {
			glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
			Ref<Texture2D> Texture;
			float TilingFactor = 1.0f;
		};

		struct CircleSpec {
			glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
			float Thickness = 1.0f;
			float Fade = 0.005f;
		};

		enum class ShapeType {
			Quad,
			Circle
		};

		ShapeType Type = ShapeType::Quad;

		QuadSpec RectDetails;
		CircleSpec CircleDetails;

		Shape2DComponent() = default;
		Shape2DComponent(const Shape2DComponent&) = default;
		Shape2DComponent(ShapeType type, const glm::vec4& color);

		static ShapeType ShapeTypeFromString(const std::string& shapeTypeStr) {
			std::string shapeType = shapeTypeStr;
			std::transform(shapeType.begin(), shapeType.end(), shapeType.begin(), ::tolower);

			if (shapeType == "quad") return ShapeType::Quad;
			if (shapeType == "circle") return ShapeType::Circle;

			W_CORE_ASSERT(false, "Unknown body type while deserializing");
			return ShapeType::Quad;
		}

		static std::string ShapeTypeToString(ShapeType shapeType) {
			switch (shapeType) {
				case ShapeType::Quad:    return "Quad";
				case ShapeType::Circle:   return "Circle";
				default:
					W_CORE_ASSERT(false, "Unknown body type while serializing");
					return "Quad";
			}
		}
	};
}
