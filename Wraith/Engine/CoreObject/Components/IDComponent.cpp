#include "wpch.h"
#include "IDComponent.h"
#include "YAMLOperators.h"

#include <imgui.h>

namespace Wraith {
	void IDComponent::Serialize(YAML::Emitter& out) const {
		out << YAML::Key << "Entity" << YAML::Value << ID;
	}

	void IDComponent::Deserialize(const YAML::Node& node) {
		ID = node["Entity"].as<Guid>();
	}

	void IDComponent::DrawImGuiProperties() {
		char buffer[256];
		strcpy_s(buffer, sizeof(buffer), ID.ToString().c_str());
		ImGui::Text(buffer);
	}
}
