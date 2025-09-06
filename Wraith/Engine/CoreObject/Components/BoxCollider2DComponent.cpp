#include "wpch.h"
#include "BoxCollider2DComponent.h"
#include "YAMLOperators.h"
#include <imgui.h>

namespace Wraith {
	void BoxCollider2DComponent::Serialize(YAML::Emitter& out) const {
		out << YAML::Key << "Offset" << YAML::Value << Offset;
		out << YAML::Key << "Size" << YAML::Value << Size;
		out << YAML::Key << "Density" << YAML::Value << Density;
		out << YAML::Key << "Friction" << YAML::Value << Friction;
		out << YAML::Key << "Restitution" << YAML::Value << Restitution;
		out << YAML::Key << "RestitutionThreshold" << YAML::Value << RestitutionThreshold;
	}

	void BoxCollider2DComponent::Deserialize(const YAML::Node& node) {
		Offset = node["Offset"].as<glm::vec2>();
		Size = node["Size"].as<glm::vec2>();
		Density = node["Density"].as<float>();
		Friction = node["Friction"].as<float>();
		Restitution = node["Restitution"].as<float>();
		RestitutionThreshold = node["RestitutionThreshold"].as<float>();
	}

	void BoxCollider2DComponent::DrawImGuiProperties() {
		ImGui::DragFloat2("Offset", &Offset.x, 0.01f);
		ImGui::DragFloat2("Size", &Size.x, 0.01f, 0.0f, 0.0f, "%.2f");

		ImGui::Separator();
		ImGui::Text("Physics Material");

		ImGui::DragFloat("Density", &Density, 0.01f, 0.0f, 0.0f, "%.2f");
		ImGui::DragFloat("Friction", &Friction, 0.01f, 0.0f, 1.0f, "%.2f");
		ImGui::DragFloat("Restitution", &Restitution, 0.01f, 0.0f, 1.0f, "%.2f");
		ImGui::DragFloat("Restitution Threshold", &RestitutionThreshold, 0.01f, 0.0f, 0.0f, "%.2f");
	}
}
