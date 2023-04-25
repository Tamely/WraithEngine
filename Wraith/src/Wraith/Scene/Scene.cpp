#include "wpch.h"
#include "Scene.h"

#include "Components.h"
#include "Wraith/Renderer/Renderer2D.h"

#include "Entity.h"

namespace Wraith {
	Scene::Scene() {
	}

	Scene::~Scene() {

	}

	Entity Scene::CreateEntity(const std::string& name) {
		Entity entity = { m_Registry.create(), this };

		entity.AddComponent<TransformComponent>();

		auto& tag = entity.AddComponent<TagComponent>();
		tag.Tag = name.empty() ? "Unnamed_Entity" : name;

		return entity;
	}

	void Scene::OnUpdate(Timestep ts) {
		auto textureGroup = m_Registry.group<TransformComponent>(entt::get<TextureComponent, SpriteRendererComponent>);
		for (auto entity : textureGroup) {
			auto& [transform, texture, sprite] = textureGroup.get<TransformComponent, TextureComponent, SpriteRendererComponent>(entity);

			Renderer2D::DrawQuad(transform, texture.Texture, 1.0f, sprite.Color);
		}
	}
}