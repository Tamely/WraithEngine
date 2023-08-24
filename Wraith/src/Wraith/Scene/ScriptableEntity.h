#pragma once

#include "Entity.h"

namespace Wraith {
	class ScriptableEntity : public Entity {
	public:
		template<typename T>
		T& GetComponent() {
			return m_Entity.GetComponent<T>();
		}
	private:
		Entity m_Entity;
		friend class Scene;
	};
}