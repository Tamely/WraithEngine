#pragma once
#include "CoreObject/ComponentMacros.h"
#include <string>

namespace Wraith {
	struct TagComponent {
		GENERATED_COMPONENT_BODY(TagComponent);

		std::string Tag;

		TagComponent() = default;
		TagComponent(const TagComponent&) = default;
		TagComponent(const std::string& tag) : Tag(tag) {}
	};
}
