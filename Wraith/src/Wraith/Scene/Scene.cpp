#include "wpch.h"
#include "Scene.h"

#include "Components.h"
#include "Wraith/Renderer/Renderer2D.h"

namespace Wraith {
	Scene::Scene() {
		entt::entity entity = m_Registry.create();
	}

	Scene::~Scene() {

	}

	entt::entity Scene::CreateEntity() {
		return m_Registry.create();
	}

	void Scene::OnUpdate(Timestep ts) {
		auto group = m_Registry.group<TransformComponent>(entt::get<SpriteRendererComponent>);
		for (auto entity : group) {
			auto& [transform, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entity);

			Renderer2D::DrawQuad(transform, sprite.Color);
		}
	}
}