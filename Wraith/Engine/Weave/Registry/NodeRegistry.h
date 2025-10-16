#pragma once

#include "Registry/NodeDefinition.h"

#include "Weave/Node.h"
#include "Weave/NodeType.h"

#include <map>
#include <string>
#include <vector>

namespace Wraith {
	class NodeRegistry {
	public:
		static NodeRegistry& Instance() {
			static NodeRegistry instance;
			return instance;
		}

		void RegisterNode(INodeDefinition* definition) {
			std::string name = definition->GetName();
			std::string category = definition->GetCategory();

			m_NodeDefinitions[name] = definition;
			m_Categories[category].push_back(name);
		}

		const std::map<std::string, std::vector<std::string>>& GetCategories() const {
			return m_Categories;
		}

		INodeDefinition* GetNodeDefinition(const std::string& name) const {
			auto it = m_NodeDefinitions.find(name);
			if (it != m_NodeDefinitions.end()) return it->second;
			return nullptr;
		}

		Node CreateNode(const std::string& definitionName, int& idCounter) const {
			INodeDefinition* def = GetNodeDefinition(definitionName);
			if (!def) {
				return Node(idCounter++, "Unknown", ImColor(255, 0, 0));
			}

			Node node(idCounter++, def->GetName().c_str(), def->GetColor());
			node.Type = def->GetNodeType();
			node.Size = def->GetDefaultSize();

			for (const auto& [pinName, pinType] : def->GetInputs()) {
				node.Inputs.emplace_back(idCounter++, pinName.c_str(), pinType);
			}

			for (const auto& [pinName, pinType] : def->GetOutputs()) {
				node.Outputs.emplace_back(idCounter++, pinName.c_str(), pinType);
			}

			return node;
		}

	private:
		NodeRegistry() = default;
		~NodeRegistry() = default;
		NodeRegistry(const NodeRegistry&) = delete;
		NodeRegistry& operator=(const NodeRegistry&) = delete;

		std::map<std::string, INodeDefinition*> m_NodeDefinitions; // Using a map over unordered_map, so we have consistent alphabetical sorting
		std::map<std::string, std::vector<std::string>> m_Categories;
	};

	template<typename T>
	class NodeAutoRegister {
	public:
		NodeAutoRegister() {
			static T instance;
			NodeRegistry::Instance().RegisterNode(&instance);
		}
	};
}

// Macro to define a node class that auto-registers on compile time
#define DEFINE_NODE_CLASS(ClassName, NodeName, Category)										\
	namespace Wraith {																			\
		class ClassName : public Wraith::INodeDefinition {										\
		public:																					\
			std::string GetName() const override { return NodeName; }							\
			std::string GetCategory() const override { return Category; }						\
			ImColor GetColor() const override;													\
			Wraith::NodeType GetNodeType() const override;										\
			std::vector<std::pair<std::string, Wraith::PinType>> GetInputs() const override;	\
			std::vector<std::pair<std::string, Wraith::PinType>> GetOutputs() const override;	\
			ImVec2 GetDefaultSize() const override { return ImVec2(0, 0); }						\
		};																						\
	}																							\
	namespace { static Wraith::NodeAutoRegister<Wraith::ClassName> g_AutoRegister_##ClassName; }

// Simplified macro for common cases
#define DEFINE_SIMPLE_NODE(ClassName, NodeName, Category)										\
	DEFINE_NODE_CLASS(ClassName, NodeName, Category)
