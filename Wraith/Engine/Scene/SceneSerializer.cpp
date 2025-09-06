#include "wpch.h"

#include "Scene/SceneSerializer.h"
#include "CoreObject/Entity.h"
#include "ComponentMacros.h"
#include "Components/IDComponent.h"
#include "Components/TransformComponent.h"
#include "YAMLOperators.h"

#include <fstream>
#include <yaml-cpp/yaml.h>

namespace Wraith {

	SceneSerializer::SceneSerializer(const Ref<Scene>& scene) : m_Scene(scene) {}

	static void SerializeEntity(YAML::Emitter& out, Entity entity) {
		W_CORE_ASSERT(entity.HasComponent<IDComponent>());

		out << YAML::BeginMap; // Entity
		entity.GetComponent<IDComponent>().Serialize(out);

		for (const auto& [name, info] : ComponentRegistry::GetComponents()) {
			if (name == "IDComponent") continue; // We serialize this as Entity, not a component

			info.serialize(entity, out);
		}

		out << YAML::EndMap; // Entity
	}

	void SceneSerializer::Serialize(const std::string& filepath) {
		YAML::Emitter out;
		out << YAML::BeginMap; // Scene
		out << YAML::Key << "Scene" << YAML::Value << "Untitled";
		out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq; // Entities

		m_Scene->m_Registry.each([&](auto entityID) {
			Entity entity = { entityID, m_Scene.get() };
			if (!entity) return;

			SerializeEntity(out, entity);
		});

		out << YAML::EndSeq; // Entities
		out << YAML::EndMap; // Scene

		std::ofstream ofs(filepath);
		ofs << out.c_str();
	}

	void SceneSerializer::SerializeRuntime(const std::string& filepath) {
		// Not implemented
		W_CORE_ASSERT(false);
	}

	bool SceneSerializer::Deserialize(const std::string& filepath) {
		std::ifstream stream(filepath);
		std::stringstream sstream;
		sstream << stream.rdbuf();

		YAML::Node data = YAML::Load(sstream.str());
		if (!data["Scene"]) return false;

		std::string sceneName = data["Scene"].as<std::string>();
		W_CORE_TRACE("Deserializing scene '{0}'", sceneName);

		auto entities = data["Entities"];
		if (entities) {
			for (auto entity : entities) {
				Guid guid = entity["Entity"].as<Guid>();

				std::string name;
				auto tagComponent = entity["TagComponent"];
				if (tagComponent) name = tagComponent["Tag"].as<std::string>();

				W_CORE_TRACE("Deserialized entity with ID = {0}, name = {1}", guid.ToString(), name);

				Entity deserializedEntity = m_Scene->CreateEntity(guid, name);

				auto transformComponent = entity["TransformComponent"];
				if (transformComponent) {
					deserializedEntity.GetComponent<TransformComponent>().Deserialize(transformComponent);
				}

				for (const auto& [componentName, info] : ComponentRegistry::GetComponents()) {
					if (componentName == "TagComponent" || componentName == "TransformComponent") continue;

					auto componentNode = entity[componentName];
					if (componentNode) {
						info.deserialize(deserializedEntity, componentNode);
					}
				}
			}
		}

		return true;
	}

	bool SceneSerializer::DeserializeRuntime(const std::string& filepath) {
		// Not implemented
		W_CORE_ASSERT(false);
		return false;
	}
}
