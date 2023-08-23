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
		// Render 2D

		// Camera
		Camera* mainCamera = nullptr;
		glm::mat4* mainTransform = nullptr;
		{
			auto group = m_Registry.view<TransformComponent, CameraComponent>();
			for (auto entity : group) {
				auto& [transform, camera] = group.get<TransformComponent, CameraComponent>(entity);

				if (camera.Primary) {
					mainCamera = &camera.Camera;
					mainTransform = &transform.Transform;
					break;
				}
			}
		}

		if (mainCamera) {
			Renderer2D::BeginScene(*mainCamera, *mainTransform);

			// Textures
			{
				auto group = m_Registry.group<TransformComponent>(entt::get<TextureComponent, SpriteRendererComponent>);
				for (auto entity : group) {
					auto& [transform, texture, sprite] = group.get<TransformComponent, TextureComponent, SpriteRendererComponent>(entity);

					Renderer2D::DrawQuad(transform, texture.Texture, 1.0f, sprite.Color);
				}
			}

			Renderer2D::EndScene();
		}
	}
}