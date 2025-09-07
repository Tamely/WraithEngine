#include "wpch.h"
#include "RigidBody2DComponent.h"

#include "UI/UIRenderCommand.h"

namespace Wraith {
	void RigidBody2DComponent::Serialize(YAML::Emitter& out) const {
		out << YAML::Key << "Type" << YAML::Value << BodyTypeToString(Type);
		out << YAML::Key << "FixedRotation" << YAML::Value << FixedRotation;
	}

	void RigidBody2DComponent::Deserialize(const YAML::Node& node) {
		std::string typeStr = node["Type"].as<std::string>();
		Type = BodyTypeFromString(typeStr);
		FixedRotation = node["FixedRotation"].as<bool>();
	}

	void RigidBody2DComponent::DrawImGuiProperties() {
		const char* bodyTypeStrings[] = { "Static", "Dynamic", "Kinematic" };
		const char* currentBodyTypeString = bodyTypeStrings[(int)Type];

		if (UIRenderCommand::BeginCombo("Body Type", currentBodyTypeString)) {
			for (int i = 0; i < 3; i++) {
				bool isSelected = currentBodyTypeString == bodyTypeStrings[i];
				if (UIRenderCommand::Selectable(bodyTypeStrings[i], isSelected)) {
					currentBodyTypeString = bodyTypeStrings[i];
					Type = (BodyType)i;
				}

				if (isSelected) UIRenderCommand::SetItemDefaultFocus();
			}
			UIRenderCommand::EndCombo();
		}

		UIRenderCommand::Checkbox("Fixed Rotation", &FixedRotation);
	}
}
