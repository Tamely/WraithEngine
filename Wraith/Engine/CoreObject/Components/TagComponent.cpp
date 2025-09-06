#include "wpch.h"
#include "TagComponent.h"
#include <imgui.h>

namespace Wraith {
	void TagComponent::Serialize(YAML::Emitter& out) const {
		out << YAML::Key << "Tag" << YAML::Value << Tag;
	}

	void TagComponent::Deserialize(const YAML::Node& node) {
		Tag = node["Tag"].as<std::string>();
	}

	void TagComponent::DrawImGuiProperties() {
		char buffer[256];
		strcpy_s(buffer, sizeof(buffer), Tag.c_str());
		if (ImGui::InputText("##Tag", buffer, sizeof(buffer))) {
			Tag = std::string(buffer);
		}
	}
}
