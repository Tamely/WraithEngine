#include "wpch.h"
#include "SpriteRendererComponent.h"
#include "CoreObject/YAMLOperators.h"
#include "PayloadDefinitions.h"

#include <imgui.h>

#include <filesystem>

namespace Wraith {
	extern const std::filesystem::path g_ContentDirectory;

	void SpriteRendererComponent::Serialize(YAML::Emitter& out) const {
		out << YAML::Key << "Color" << YAML::Value << Color;
		out << YAML::Key << "TilingFactor" << YAML::Value << TilingFactor;
		// Note: Texture serialization would need additional handling for asset references
	}

	void SpriteRendererComponent::Deserialize(const YAML::Node& node) {
		Color = node["Color"].as<glm::vec4>();
		if (node["TilingFactor"]) {
			TilingFactor = node["TilingFactor"].as<float>();
		}
		// Note: Texture deserialization would need additional handling for asset references
	}

	void SpriteRendererComponent::DrawImGuiProperties() {
		ImGui::ColorEdit4("Color", &Color.x);
		ImGui::DragFloat("Tiling Factor", &TilingFactor, 0.1f, 0.0f, 0.0f, "%.1f");

		ImGui::Button("Texture", ImVec2(100.0f, 0.0f));
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_TEXTURE)) {
				const wchar_t* path = (const wchar_t*)payload->Data;
				std::filesystem::path texturePath = g_ContentDirectory / path;
				Texture = Texture2D::Create(texturePath.string(), true);
			}
			ImGui::EndDragDropTarget();
		}
	}
}
