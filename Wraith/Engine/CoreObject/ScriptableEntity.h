#pragma once

#include "CoreObject/Entity.h"

namespace Wraith {
	class ScriptableEntity : public Entity {
	public:
		virtual ~ScriptableEntity() = default;

		template<typename T>
		T& GetComponent() {
			return m_Entity.GetComponent<T>();
		}
	protected:
		virtual void OnCreate() {}
		virtual void OnDestroy() {}
		virtual void OnUpdate(Timestep ts) {}
	private:
		Entity m_Entity;
		friend class Scene;
	};
}
