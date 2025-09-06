#pragma once
#include "Misc/Guid.h"
#include "Core/Timestep.h"
#include "Editor/EditorCamera.h"

#include <entt.hpp>

namespace Wraith {
	class Entity;
	class PhysicsWorld2D;

	class Scene {
	public:
		Scene();
		~Scene();

		Entity CreateEntity(Guid& guid = Guid(), const std::string& name = std::string());
		void DestroyEntity(Entity entity);

		void OnRuntimeStart();
		void OnRuntimeStop();

		void OnUpdateEditor(Timestep ts, EditorCamera& camera);
		void OnUpdateRuntime(Timestep ts);

		void OnViewportResize(uint32_t width, uint32_t height);

		Entity GetPrimaryCameraEntity();

	public:
		template<typename T>
		void HandleComponentInitialization(Entity& entity, T& component);

	private:
		void UpdateScripts(Timestep ts);
		void UpdatePhysics(Timestep ts);
		void RenderScene();
		void RenderSceneWithCamera(class Camera& camera, const glm::mat4& transform);

	public:
		uint32_t GetViewportWidth() const { return m_ViewportWidth; }
		uint32_t GetViewportHeight() const { return m_ViewportHeight; }

	private:
		entt::registry m_Registry;
		uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;

		// Physics abstraction - will be expanded for 3D later
		std::unique_ptr<PhysicsWorld2D> m_PhysicsWorld2D;

		friend class Entity;
		friend class SceneSerializer;
		friend class SceneHierarchyPanel;
	};
}
