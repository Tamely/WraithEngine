#include "wpch.h"
#include "CameraComponent.h"
#include "CoreObject/YAMLOperators.h"

#include "UI/UIRenderCommand.h"

namespace Wraith {
	void CameraComponent::Serialize(YAML::Emitter& out) const {
		out << YAML::Key << "SceneCamera" << YAML::Value << Camera;
		out << YAML::Key << "Primary" << YAML::Value << Primary;
		out << YAML::Key << "FixedAspectRatio" << YAML::Value << FixedAspectRatio;
	}

	void CameraComponent::Deserialize(const YAML::Node& node) {
		Camera = node["SceneCamera"].as<SceneCamera>();
		Primary = node["Primary"].as<bool>();
		FixedAspectRatio = node["FixedAspectRatio"].as<bool>();
	}

	void CameraComponent::DrawImGuiProperties() {
		UIRenderCommand::Checkbox("Primary", &Primary);
		UIRenderCommand::Checkbox("Fixed Aspect Ratio", &FixedAspectRatio);

		const char* projectionTypeStrings[] = { "Perspective", "Orthographic" };
		const char* currentProjectionTypeString = projectionTypeStrings[(int)Camera.GetProjectionType()];

		if (UIRenderCommand::BeginCombo("Projection", currentProjectionTypeString)) {
			for (int i = 0; i < 2; i++) {
				bool isSelected = currentProjectionTypeString == projectionTypeStrings[i];
				if (UIRenderCommand::Selectable(projectionTypeStrings[i], isSelected)) {
					currentProjectionTypeString = projectionTypeStrings[i];
					Camera.SetProjectionType((SceneCamera::ProjectionType)i);
				}

				if (isSelected) UIRenderCommand::SetItemDefaultFocus();
			}
			UIRenderCommand::EndCombo();
		}

		if (Camera.GetProjectionType() == SceneCamera::ProjectionType::Perspective) {
			float perspectiveVerticalFov = glm::degrees(Camera.GetPerspectiveVerticalFOV());
			if (UIRenderCommand::DragFloat("Vertical FOV", &perspectiveVerticalFov)) Camera.SetPerspectiveVerticalFOV(glm::radians(perspectiveVerticalFov));

			float perspectiveNear = Camera.GetPerspectiveNearClip();
			if (UIRenderCommand::DragFloat("Near", &perspectiveNear)) Camera.SetPerspectiveNearClip(perspectiveNear);

			float perspectiveFar = Camera.GetPerspectiveFarClip();
			if (UIRenderCommand::DragFloat("Far", &perspectiveFar)) Camera.SetPerspectiveFarClip(perspectiveFar);
		}

		if (Camera.GetProjectionType() == SceneCamera::ProjectionType::Orthographic) {
			float orthoSize = Camera.GetOrthographicSize();
			if (UIRenderCommand::DragFloat("Size", &orthoSize)) Camera.SetOrthographicSize(orthoSize);

			float orthoNear = Camera.GetOrthographicNearClip();
			if (UIRenderCommand::DragFloat("Near", &orthoNear)) Camera.SetOrthographicNearClip(orthoNear);

			float orthoFar = Camera.GetOrthographicFarClip();
			if (UIRenderCommand::DragFloat("Far", &orthoFar)) Camera.SetOrthographicFarClip(orthoFar);
		}
	}
}
