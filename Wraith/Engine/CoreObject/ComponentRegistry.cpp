#include "wpch.h"
#include "ComponentMacros.h"

// make sure every component header that uses GENERATED_COMPONENT_BODY is included
// here so that their static registration happens in this TU during program startup.
#include "Components/TagComponent.h"
#include "Components/TransformComponent.h"
#include "Components/SpriteRendererComponent.h"
#include "Components/CameraComponent.h"
#include "Components/RigidBody2DComponent.h"
#include "Components/BoxCollider2DComponent.h"

namespace Wraith {
	// Nothing else needed: registrations happen by the s_Registered inline variables
	// in each component header when this TU is loaded by the program.
	void ComponentRegistry_ForceLink() {}
}
