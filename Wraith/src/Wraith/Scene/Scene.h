#pragma once

#include "entt.hpp"

#include "Wraith/Core/Timestep.h"

namespace Wraith {
	class Scene {
	public:
		Scene();
		~Scene();

		entt::entity CreateEntity();

		// TEMP
		entt::registry& GetRegistry() { return m_Registry; }

		void OnUpdate(Timestep ts);
	private:
		entt::registry m_Registry;
	};
}