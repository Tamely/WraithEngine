#include "wpch.h"
#include "NodeGraph.h"

#include <Core/Log.h>
#include <algorithm>

namespace Wraith {
	uint32_t NodeGraph::CreateNode(const std::string& nodeType, glm::vec2 position) {
		uint32_t nodeID = GetNextNodeID();

		Node node;
		node.ID = nodeID;
		node.Title = nodeType;
		node.NodeType = nodeType;
		node.Position = position;

		SetupNodePins(node);
		node.UpdatePinPositions();

		m_Nodes[nodeID] = std::move(node);

		W_INFO("Created node '{}' with ID {}", nodeType, nodeID);
		return nodeID;
	}

	void NodeGraph::DeleteNode(uint32_t nodeID) {
		auto nodeIt = m_Nodes.find(nodeID);
		if (nodeIt == m_Nodes.end()) return;

		const Node& node = nodeIt->second;

		// Delete all connections to this node's pins
		std::vector<uint32_t> connectionsToDelete;
		for (const auto& [connID, connection] : m_Connections) {
			bool fromBelongsToNode = node.GetPin(connection.FromPinID) != nullptr;
			bool toBelongsToNode = node.GetPin(connection.ToPinID) != nullptr;

			if (fromBelongsToNode || toBelongsToNode) {
				connectionsToDelete.push_back(connID);
			}
		}

		for (uint32_t connID : connectionsToDelete) {
			DeleteConnection(connID);
		}

		m_Nodes.erase(nodeIt);
		W_INFO("Deleted node with ID: {}", nodeID);
	}

	uint32_t NodeGraph::CreateConnection(uint32_t fromPinID, uint32_t toPinID) {
		if (!CanConnect(fromPinID, toPinID)) {
			return 0;
		}

		uint32_t connectionID = GetNextConnectionID();

		Connection connection;
		connection.ID = connectionID;
		connection.FromPinID = fromPinID;
		connection.ToPinID = toPinID;

		// Add connection references to pins
		Pin* fromPin = FindPin(fromPinID);
		Pin* toPin = FindPin(toPinID);

		if (fromPin && toPin) {
			fromPin->ConnectedPins.push_back(toPinID);
			toPin->ConnectedPins.push_back(fromPinID);

			m_Connections[connectionID] = std::move(connection);
			W_INFO("Created connection {} -> {}", fromPinID, toPinID);
			return connectionID;
		}

		return 0;
	}

	void NodeGraph::DeleteConnection(uint32_t connectionID) {
		auto connIt = m_Connections.find(connectionID);
		if (connIt == m_Connections.end()) return;

		const Connection& connection = connIt->second;

		// Remvoe connection references from pins
		Pin* fromPin = FindPin(connection.FromPinID);
		Pin* toPin = FindPin(connection.ToPinID);

		if (fromPin) {
			auto& connections = fromPin->ConnectedPins;
			connections.erase(std::remove(connections.begin(), connections.end(), connection.ToPinID), connections.end());
		}

		if (toPin) {
			auto& connections = toPin->ConnectedPins;
			connections.erase(std::remove(connections.begin(), connections.end(), connection.FromPinID), connections.end());
		}

		m_Connections.erase(connIt);
		W_INFO("Deleted connection {}", connectionID);
	}

	bool NodeGraph::CanConnect(uint32_t fromPinID, uint32_t toPinID) {
		const Pin* fromPin = FindPin(fromPinID);
		const Pin* toPin = FindPin(toPinID);

		if (!fromPin || !toPin) return false;

		if (fromPin == toPin) return false; // Can't connect to same pin
		if (fromPin->IsInput && toPin->IsInput) return false; // Must connect input to output (or vice versa)

		// Ensure fromPin is output and toPin is input
		if (fromPin->IsInput) {
			std::swap(fromPin, toPin);
			std::swap(fromPinID, toPinID);
		}

		for (const auto& [nodeID, node] : m_Nodes) {
			bool hasFromPin = node.GetPin(fromPinID) != nullptr;
			bool hasToPin = node.GetPin(toPinID) != nullptr;
			if (hasFromPin && hasToPin) return false; // Can't connect pins from the same node
		}

		if (toPin->IsInput && !toPin->ConnectedPins.empty()) return false; // Input pins can only have one connection

		// TODO: Add type checking here (smt like fromPin->Type != toPin->Type)

		return true;
	}

	void NodeGraph::UpdateNodePositions() {
		for (auto& [nodeID, node] : m_Nodes) {
			node.UpdatePinPositions();
		}
	}

	Node* NodeGraph::GetNode(uint32_t id) {
		auto it = m_Nodes.find(id);
		return it != m_Nodes.end() ? &it->second : nullptr;
	}

	const Node* NodeGraph::GetNode(uint32_t id) const {
		auto it = m_Nodes.find(id);
		return it != m_Nodes.end() ? &it->second : nullptr;
	}

	Connection* NodeGraph::GetConnection(uint32_t id) {
		auto it = m_Connections.find(id);
		return it != m_Connections.end() ? &it->second : nullptr;
	}

	const Connection* NodeGraph::GetConnection(uint32_t id) const {
		auto it = m_Connections.find(id);
		return it != m_Connections.end() ? &it->second : nullptr;
	}

	Pin* NodeGraph::FindPin(uint32_t pinID) {
		for (auto& [nodeID, node] : m_Nodes) {
			Pin* pin = node.GetPin(pinID);
			if (pin) return pin;
		}
		return nullptr;
	}

	const Pin* NodeGraph::FindPin(uint32_t pinID) const {
		for (const auto& [nodeID, node] : m_Nodes) {
			const Pin* pin = node.GetPin(pinID);
			if (pin) return pin;
		}
		return nullptr;
	}

	void NodeGraph::SetupNodePins(Node& node) {
		// TODO: Make a registry for nodes
		if (node.NodeType == "Add") {
			// Input pins
			Pin inputA;
			inputA.ID = GetNextPinID();
			inputA.Name = "A";
			inputA.Type = PinType::Float;
			inputA.IsInput = true;
			inputA.Value = 0.0f;
			node.InputPins.push_back(inputA);

			Pin inputB;
			inputB.ID = GetNextPinID();
			inputB.Name = "B";
			inputB.Type = PinType::Float;
			inputB.IsInput = true;
			inputB.Value = 0.0f;
			node.InputPins.push_back(inputB);

			// Output pin
			Pin output;
			output.ID = GetNextPinID();
			output.Name = "Result";
			output.Type = PinType::Float;
			output.IsInput = false;
			node.OutputPins.push_back(output);

			node.Color = { 0.3f, 0.5f, 0.3f, 1.0f }; // Green tint for math nodes
		}
		else {
			// Default node - generic input/output
			Pin input;
			input.ID = GetNextPinID();
			input.Name = "Input";
			input.Type = PinType::Float;
			input.IsInput = true;
			node.InputPins.push_back(input);

			Pin output;
			output.ID = GetNextPinID();
			output.Name = "Output";
			output.Type = PinType::Float;
			output.IsInput = false;
			node.OutputPins.push_back(output);

			node.Color = { 0.3f, 0.3f, 0.35f, 1.0f }; // Default gray
		}
	}
}
