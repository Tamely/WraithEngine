#include "wpch.h"
#include "Shape2DComponent.h"
#include "CoreObject/YAMLOperators.h"
#include "PayloadDefinitions.h"

#include "UI/UIRenderCommand.h"
#include "UI/UIDragDropPayload.h"

#include <filesystem>

namespace Wraith {
	extern const std::filesystem::path g_ContentDirectory;

	void Shape2DComponent::Serialize(YAML::Emitter& out) const {
		out << YAML::Key << "Type" << YAML::Value << ShapeTypeToString(Type);

		switch (Type) {
			case ShapeType::Quad: {
				out << YAML::Key << "Color" << YAML::Value << RectDetails.Color;
				out << YAML::Key << "TilingFactor" << YAML::Value << RectDetails.TilingFactor;
				// Note: Texture serialization would need additional handling for asset references
				break;
			}
			case ShapeType::Circle: {
				out << YAML::Key << "Color" << YAML::Value << CircleDetails.Color;
				out << YAML::Key << "Thickness" << YAML::Value << CircleDetails.Thickness;
				out << YAML::Key << "Fade" << YAML::Value << CircleDetails.Fade;
				break;
			}
		}
	}

	void Shape2DComponent::Deserialize(const YAML::Node& node) {
		Type = ShapeTypeFromString(node["Type"].as<std::string>());

		switch (Type) {
			case ShapeType::Quad: {
				RectDetails.Color = node["Color"].as<glm::vec4>();
				if (node["TilingFactor"]) {
					RectDetails.TilingFactor = node["TilingFactor"].as<float>();
				}
				// Note: Texture deserialization would need additional handling for asset references
				break;
			}
			case ShapeType::Circle: {
				CircleDetails.Color = node["Color"].as<glm::vec4>();
				CircleDetails.Thickness = node["Thickness"].as<float>();
				CircleDetails.Fade = node["Fade"].as<float>();
				break;
			}
		}
	}

	void Shape2DComponent::DrawImGuiProperties() {
		const char* shapeTypeStrings[] = { "Quad", "Circle" };
		const char* currentShapeTypeString = shapeTypeStrings[(int)Type];

		if (UIRenderCommand::BeginCombo("Shape Type", currentShapeTypeString)) {
			for (int i = 0; i < 2; i++) {
				bool isSelected = currentShapeTypeString == shapeTypeStrings[i];
				if (UIRenderCommand::Selectable(shapeTypeStrings[i], isSelected)) {
					currentShapeTypeString = shapeTypeStrings[i];
					Type = (ShapeType)i;
				}

				if (isSelected) UIRenderCommand::SetItemDefaultFocus();
			}
			UIRenderCommand::EndCombo();
		}

		switch (Type) {
			case ShapeType::Quad: {
				UIRenderCommand::ColorEdit4("Color", &RectDetails.Color.x);
				UIRenderCommand::DragFloat("Tiling Factor", &RectDetails.TilingFactor, 0.1f, 0.0f, 0.0f, "%.1f");

				UIRenderCommand::Button("Texture", glm::vec2{ 100.0f, 0.0f });
				if (UIRenderCommand::BeginDragDropTarget()) {
					if (const UIDragDropPayload* payload = UIRenderCommand::AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_TEXTURE)) {
						const wchar_t* path = (const wchar_t*)payload->Data;
						std::filesystem::path texturePath = g_ContentDirectory / path;
						RectDetails.Texture = Texture2D::Create(texturePath.string(), true);
					}
					UIRenderCommand::EndDragDropTarget();
				}
				break;
			}
			case ShapeType::Circle: {
				UIRenderCommand::ColorEdit4("Color", &CircleDetails.Color.x);
				UIRenderCommand::DragFloat("Thickness", &CircleDetails.Thickness, 0.01f, 0.0f, 1.0f, "%.2f");
				UIRenderCommand::DragFloat("Fade", &CircleDetails.Fade, 0.01f, 0.0f, 10.0f, "%.2f");
				break;
			}
		}
	}

	Shape2DComponent::Shape2DComponent(ShapeType type, const glm::vec4& color) {
		Type = type;
		switch (type) {
			case ShapeType::Quad:
				RectDetails.Color = color;
				break;
			case ShapeType::Circle:
				CircleDetails.Color = color;
				break;
		}
	}
}
