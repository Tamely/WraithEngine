#pragma once
#include "ComponentMacros.h"
#include "Scene/SceneCamera.h"

namespace Wraith {
	struct CameraComponent {
		GENERATED_COMPONENT_BODY(CameraComponent);

		SceneCamera Camera;
		bool Primary = true;
		bool FixedAspectRatio = false;

		CameraComponent() = default;
		CameraComponent(const CameraComponent&) = default;

		bool operator==(const CameraComponent& other) const {
			return Camera.GetProjection() == other.Camera.GetProjection()
				&& Camera.GetProjectionType() == other.Camera.GetProjectionType()
				&& Primary == other.Primary
				&& FixedAspectRatio == other.FixedAspectRatio;
		}

		bool operator!=(const CameraComponent& other) const {
			return !(*this == other);
		}
	};
}
