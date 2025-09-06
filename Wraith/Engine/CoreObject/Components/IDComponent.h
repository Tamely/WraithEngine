#pragma once
#include "ComponentMacros.h"
#include "Misc/Guid.h"

namespace Wraith {
	struct IDComponent {
		GENERATED_COMPONENT_BODY(IDComponent);

		Guid ID;

		IDComponent() = default;
		IDComponent(const IDComponent&) = default;
	};
}
