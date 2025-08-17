#pragma once

#include "Scene.h"
#include "Wraith/Core/Log.h"

#include "entt.hpp"

namespace Wraith {
	class Entity {
	public:
		Entity() = default;
		Entity(entt::entity handle, Scene* scene);
		Entity(const Entity& other) = default;

		template <typename T, typename... Args>
		void BindNativeScript(Args&&... args) {
			AddComponent<NativeScriptComponent>(std::forward<Args>(args)...).Bind<T>();
		}

		template<typename T, typename... Args>
		T& AddComponent(Args&&... args) {
			W_CORE_ASSERT(!HasComponent<T>(), "Entity already has component!");
			T& component = m_Scene->m_Registry.emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
			m_Scene->OnComponentAdded<T>(*this, component);
			return component;
		}

		template<typename T>
		T& GetComponent() {
			W_CORE_ASSERT(HasComponent<T>(), "Entity does not have component!");
			return m_Scene->m_Registry.get<T>(m_EntityHandle);
		}

		template<typename T>
		bool HasComponent() {
			return m_Scene->m_Registry.all_of<T>(m_EntityHandle);
		}

		template<typename T>
		void RemoveComponent() {
			W_CORE_ASSERT(HasComponent<T>(), "Entity does not have component!");
			m_Scene->m_Registry.remove<T>(m_EntityHandle);
		}

		operator bool() const { return m_EntityHandle != entt::null; }
		operator entt::entity() const { return m_EntityHandle; }
		operator uint32_t() const { return (uint32_t)m_EntityHandle; }

		bool operator==(const Entity& other) const { return m_EntityHandle == other.m_EntityHandle && m_Scene == other.m_Scene; }
		bool operator!=(const Entity& other) const { return m_EntityHandle != other.m_EntityHandle || m_Scene != other.m_Scene; }
	private:
		entt::entity m_EntityHandle = entt::null;
		Scene* m_Scene = nullptr;
	};
}
