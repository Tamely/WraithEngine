#include "wpch.h"
#include "NativeScriptComponent.h"
#include "CoreObject/YAMLOperators.h"
#include <imgui.h>

namespace Wraith {
	void NativeScriptComponent::Serialize(YAML::Emitter& out) const {}

	void NativeScriptComponent::Deserialize(const YAML::Node& node) {}

	void NativeScriptComponent::DrawImGuiProperties() {
		// We do not make this available publically
	}
}
