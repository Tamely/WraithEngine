#include "wpch.h"
#include "CameraComponent.h"
#include "CoreObject/YAMLOperators.h"
#include <imgui.h>

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
		ImGui::Checkbox("Primary", &Primary);
		ImGui::Checkbox("Fixed Aspect Ratio", &FixedAspectRatio);

		const char* projectionTypeStrings[] = { "Perspective", "Orthographic" };
		const char* currentProjectionTypeString = projectionTypeStrings[(int)Camera.GetProjectionType()];

		if (ImGui::BeginCombo("Projection", currentProjectionTypeString)) {
			for (int i = 0; i < 2; i++) {
				bool isSelected = currentProjectionTypeString == projectionTypeStrings[i];
				if (ImGui::Selectable(projectionTypeStrings[i], isSelected)) {
					currentProjectionTypeString = projectionTypeStrings[i];
					Camera.SetProjectionType((SceneCamera::ProjectionType)i);
				}

				if (isSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		if (Camera.GetProjectionType() == SceneCamera::ProjectionType::Perspective) {
			float perspectiveVerticalFov = glm::degrees(Camera.GetPerspectiveVerticalFOV());
			if (ImGui::DragFloat("Vertical FOV", &perspectiveVerticalFov))
				Camera.SetPerspectiveVerticalFOV(glm::radians(perspectiveVerticalFov));

			float perspectiveNear = Camera.GetPerspectiveNearClip();
			if (ImGui::DragFloat("Near", &perspectiveNear))
				Camera.SetPerspectiveNearClip(perspectiveNear);

			float perspectiveFar = Camera.GetPerspectiveFarClip();
			if (ImGui::DragFloat("Far", &perspectiveFar))
				Camera.SetPerspectiveFarClip(perspectiveFar);
		}

		if (Camera.GetProjectionType() == SceneCamera::ProjectionType::Orthographic) {
			float orthoSize = Camera.GetOrthographicSize();
			if (ImGui::DragFloat("Size", &orthoSize))
				Camera.SetOrthographicSize(orthoSize);

			float orthoNear = Camera.GetOrthographicNearClip();
			if (ImGui::DragFloat("Near", &orthoNear))
				Camera.SetOrthographicNearClip(orthoNear);

			float orthoFar = Camera.GetOrthographicFarClip();
			if (ImGui::DragFloat("Far", &orthoFar))
				Camera.SetOrthographicFarClip(orthoFar);
		}
	}
}
