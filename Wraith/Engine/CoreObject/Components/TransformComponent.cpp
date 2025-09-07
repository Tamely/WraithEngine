#include "wpch.h"
#include "TransformComponent.h"
#include "CoreObject/YAMLOperators.h"

#include "UI/UIRenderCommand.h"

namespace Wraith {
	void TransformComponent::Serialize(YAML::Emitter& out) const {
		out << YAML::Key << "Translation" << YAML::Value << Translation;
		out << YAML::Key << "Rotation" << YAML::Value << Rotation;
		out << YAML::Key << "Scale" << YAML::Value << Scale;
	}

	void TransformComponent::Deserialize(const YAML::Node& node) {
		Translation = node["Translation"].as<glm::vec3>();
		Rotation = node["Rotation"].as<glm::vec3>();
		Scale = node["Scale"].as<glm::vec3>();
	}

	void TransformComponent::DrawImGuiProperties() {
		UIRenderCommand::Vec3Control("Translation", Translation);
		
		glm::vec3 rotation = glm::degrees(Rotation);
		UIRenderCommand::Vec3Control("Rotation", rotation);
		Rotation = glm::radians(rotation);

		UIRenderCommand::Vec3Control("Scale", Scale, 1.0f);
	}
}
