#pragma once
#include "ComponentMacros.h"
#include "Core/CoreBasic.h"
#include <string>
#include <algorithm>

namespace Wraith {
	struct RigidBody2DComponent {
		GENERATED_COMPONENT_BODY(RigidBody2DComponent);

		enum class BodyType {
			Static = 0,
			Dynamic,
			Kinematic
		};

		BodyType Type = BodyType::Static;
		bool FixedRotation = false;
		void* RuntimeBody = nullptr; // Storage for runtime

		RigidBody2DComponent() = default;
		RigidBody2DComponent(const RigidBody2DComponent&) = default;

		static BodyType BodyTypeFromString(const std::string& bodyTypeStr) {
			std::string bodyType = bodyTypeStr;
			std::transform(bodyType.begin(), bodyType.end(), bodyType.begin(), ::tolower);

			if (bodyType == "static") return BodyType::Static;
			if (bodyType == "dynamic") return BodyType::Dynamic;
			if (bodyType == "kinematic") return BodyType::Kinematic;

			W_CORE_ASSERT(false, "Unknown body type while deserializing");
			return BodyType::Static;
		}

		static std::string BodyTypeToString(BodyType bodyType) {
			switch (bodyType) {
			case BodyType::Static:    return "Static";
			case BodyType::Dynamic:   return "Dynamic";
			case BodyType::Kinematic: return "Kinematic";
			default:
				W_CORE_ASSERT(false, "Unknown body type while serializing");
				return "Static";
			}
		}
	};
}
