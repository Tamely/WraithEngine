#include "wpch.h"
#include "BoxCollider2DComponent.h"
#include "YAMLOperators.h"

#include "UI/UIRenderCommand.h"

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
		UIRenderCommand::Vec2Control("Offset", Offset);
		UIRenderCommand::Vec2Control("Size", Size);

		UIRenderCommand::Separator();
		UIRenderCommand::Text("Physics Material");

		UIRenderCommand::DragFloat("Density", &Density, 0.01f, 0.0f, 0.0f, "%.2f");
		UIRenderCommand::DragFloat("Friction", &Friction, 0.01f, 0.0f, 1.0f, "%.2f");
		UIRenderCommand::DragFloat("Bounciness", &Restitution, 0.01f, 0.0f, 1.0f, "%.2f");
		UIRenderCommand::DragFloat("Bounciness Threshold", &RestitutionThreshold, 0.01f, 0.0f, 0.0f, "%.2f");
	}
}
