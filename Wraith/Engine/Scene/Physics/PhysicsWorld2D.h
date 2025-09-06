#pragma once

#include "Entity.h"
#include "Core/Timestep.h"

#include <Box2D/b2_world.h>

namespace Wraith {
	class PhysicsWorld2D {
	public:
		PhysicsWorld2D();
		~PhysicsWorld2D();

		void Initialize();
		void Shutdown();

		void CreateRigidBody(Entity entity);
		void UpdateTransforms(entt::registry& registry);
		void Step(Timestep ts);

	private:
		b2World* m_World = nullptr;
		friend class Scene;
	};
}
