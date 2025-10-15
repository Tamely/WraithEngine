#pragma once

#include "Core/Node.h"
#include "Core/Connection.h"

#include <map>
#include <unordered_map>

namespace Wraith {
	class NodeGraph {
	public:
		uint32_t CreateNode(const std::string& nodeType, glm::vec2 position);
		void DeleteNode(uint32_t nodeID);

		uint32_t CreateConnection(uint32_t fromPinID, uint32_t toPinID);
		void DeleteConnection(uint32_t connectionID);

		bool CanConnect(uint32_t fromPinID, uint32_t toPinID);

		void UpdateNodePositions();

	public:
		Node* GetNode(uint32_t id);
		const Node* GetNode(uint32_t id) const;

		Connection* GetConnection(uint32_t id);
		const Connection* GetConnection(uint32_t id) const;

		Pin* FindPin(uint32_t pinID);
		const Pin* FindPin(uint32_t pinID) const;

		const auto& GetNodes() const { return m_Nodes; }
		const auto& GetConnections() const { return m_Connections; }
	private:
		uint32_t GetNextNodeID() { return m_NextNodeID++; }
		uint32_t GetNextPinID() { return m_NextPinID++; }
		uint32_t GetNextConnectionID() { return m_NextConnectionID++; }

		void SetupNodePins(Node& node);
	private:
		std::map<uint32_t, Node> m_Nodes;
		std::unordered_map<uint32_t, Connection> m_Connections;
		uint32_t m_NextNodeID = 1;
		uint32_t m_NextPinID = 1000;
		uint32_t m_NextConnectionID = 10000;
	};
}
