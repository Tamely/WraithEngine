#include "wpch.h"

#include <Core/Log.h>
#include <Input/Input.h>

#include "NodeEditorInteraction.h"

namespace Wraith {
	void NodeEditorInteraction::OnMouseButtonPressed(glm::vec2 mousePos, int button) {
		glm::vec2 worldPos = m_Camera->ScreenToWorld(mousePos);
		m_LastMousePos = mousePos;
		m_DragStartPos = worldPos;

		if (button == W_MOUSE_BUTTON_1) {
			// Check if we're clicking a pin
			uint32_t pinID = GetPinUnderMouse(worldPos);
			if (pinID != 0) {
				m_State = State::ConnectingPins;
				m_DragStartPinID = pinID;
				m_ShowConnectionPreview = true;
				m_ConnectionPreviewEnd = worldPos;
				return;
			}

			// Check if we're clicking a node
			uint32_t nodeID = GetNodeUnderMouse(worldPos);
			if (nodeID != 0) {
				auto it = std::find(m_SelectedNodes.begin(), m_SelectedNodes.end(), nodeID);
				bool isAlreadySelected = (it != m_SelectedNodes.end());

				if (!isAlreadySelected) {
					if (!Input::IsKeyPressed(W_KEY_LEFT_CONTROL)) {
						DeselectAll();
					}
					SelectNode(nodeID, false);
				}
				else {
					// Node is already selected, prepare for dragging
				}

				m_State = State::DraggingNode;
				return;
			}

			// Check if we're clicking on a connection
			uint32_t connectionID = GetConnectionUnderMouse(worldPos);
			if (connectionID != 0) {
				// TODO: Handle connection selection/deselection
				return;
			}

			// Clicking empty space = start box selection
			m_State = State::BoxSelecting;
			m_BoxSelectStart = worldPos;
			m_BoxSelectEnd = worldPos;
			DeselectAll();
		}
		else if (button == W_MOUSE_BUTTON_2) {
			uint32_t nodeID = GetNodeUnderMouse(worldPos);
			uint32_t connectionID = GetConnectionUnderMouse(worldPos);

			if (nodeID != 0) {
				// TODO: Show context menu for node
				W_CORE_INFO("Right-clicked node {}", nodeID);
			}
			else if (connectionID != 0) {
				m_Graph->DeleteConnection(connectionID);
			}
			else {
				// TODO: Show context menu for creating new nodes
				W_CORE_INFO("Right-clicked empty space at ({}, {})", worldPos.x, worldPos.y);
			}
		}
		else if (button == W_MOUSE_BUTTON_3) {
			m_State = State::Panning;
		}
	}

	void NodeEditorInteraction::OnMouseButtonReleased(glm::vec2 mousePos, int button) {
		glm::vec2 worldPos = m_Camera->ScreenToWorld(mousePos);

		if (button == W_MOUSE_BUTTON_1) {
			if (m_State == State::ConnectingPins) {
				uint32_t targetPinID = GetPinUnderMouse(worldPos);
				if (targetPinID != 0 && targetPinID != m_DragStartPinID) {
					m_Graph->CreateConnection(m_DragStartPinID, targetPinID);
				}

				m_ShowConnectionPreview = false;
				m_DragStartPinID = 0;
			}
			else if (m_State == State::BoxSelecting) {
				UpdateBoxSelection();
			}
			else if (m_State == State::DraggingNode) {
				m_Graph->UpdateNodePositions();
			}
		}

		m_State = State::None;
	}

	void NodeEditorInteraction::OnMouseMoved(glm::vec2 mousePos) {
		glm::vec2 worldPos = m_Camera->ScreenToWorld(mousePos);
		glm::vec2 mouseDelta = mousePos - m_LastMousePos;
		glm::vec2 worldDelta = worldPos - m_Camera->ScreenToWorld(m_LastMousePos);

		if (m_State == State::None || m_State == State::ConnectingPins) {
			m_HoveredNodeID = GetNodeUnderMouse(worldPos);
			m_HoveredPinID = GetPinUnderMouse(worldPos);
		}

		switch (m_State) {
			case State::DraggingNode:
				for (uint32_t nodeID : m_SelectedNodes) {
					Node* node = m_Graph->GetNode(nodeID);
					if (node) {
						node->Position += worldDelta;
					}
				}
				m_Graph->UpdateNodePositions();
				break;
			case State::Panning:
				m_Camera->Pan(-worldDelta);
				break;
			case State::ConnectingPins:
				m_ConnectionPreviewEnd = worldPos;
				break;
			case State::BoxSelecting:
				m_BoxSelectEnd = worldPos;
				UpdateBoxSelection();
				break;
			default:
				break;
		}

		m_LastMousePos = mousePos;
	}

	void NodeEditorInteraction::OnScroll(float delta) {
		m_Camera->Zoom(delta);
	}

	void NodeEditorInteraction::OnKeyPressed(int key) {
		if (key == W_KEY_DELETE || key == W_KEY_BACKSPACE) {
			DeleteSelected();
		}
		else if (key == W_KEY_ESCAPE) {
			if (m_State == State::ConnectingPins) {
				m_ShowConnectionPreview = false;
				m_DragStartPinID = 0;
				m_State = State::None;
			}
			else {
				DeselectAll();
			}
		}
		else if (Input::IsKeyPressed(W_KEY_LEFT_CONTROL) && key == W_KEY_A) {
			m_SelectedNodes.clear();
			for (const auto& [nodeID, node] : m_Graph->GetNodes()) {
				m_SelectedNodes.push_back(nodeID);
			}
		}
	}

	void NodeEditorInteraction::Update() {
		for (const auto& [nodeID, node] : m_Graph->GetNodes()) {
			Node* mutableNode = m_Graph->GetNode(nodeID);
			if (mutableNode) {
				bool isSelected = std::find(m_SelectedNodes.begin(), m_SelectedNodes.end(), nodeID) != m_SelectedNodes.end();
				mutableNode->IsSelected = isSelected;
			}
		}
	}

	uint32_t NodeEditorInteraction::GetNodeUnderMouse(glm::vec2 worldPos) {
		// Get the nodes in reverse order (this is bc we want to get the last drawn first)
		for (auto it = m_Graph->GetNodes().rbegin(); it != m_Graph->GetNodes().rend(); ++it) {
			const auto& [nodeID, node] = *it;
			if (IsPointInNode(worldPos, node)) {
				return nodeID;
			}
		}

		return 0;
	}

	uint32_t NodeEditorInteraction::GetPinUnderMouse(glm::vec2 worldPos) {
		for (const auto& [nodeID, node] : m_Graph->GetNodes()) {
			// Check input pins
			for (const auto& pin : node.InputPins) {
				if (IsPointInPin(worldPos, pin)) {
					return pin.ID;
				}
			}

			// Check output pins
			for (const auto& pin : node.OutputPins) {
				if (IsPointInPin(worldPos, pin)) {
					return pin.ID;
				}
			}
		}

		return 0;
	}

	uint32_t NodeEditorInteraction::GetConnectionUnderMouse(glm::vec2 worldPos) {
		for (const auto& [connectionID, connection] : m_Graph->GetConnections()) {
			if (IsPointOnConnection(worldPos, connection)) {
				return connectionID;
			}
		}

		return 0;
	}

	bool NodeEditorInteraction::IsPointInNode(glm::vec2 point, const Node& node) {
		return	point.x >= node.Position.x &&
				point.x <= node.Position.x + node.Size.x &&
				point.y >= node.Position.y &&
				point.y <= node.Position.y + node.Size.y;
	}

	bool NodeEditorInteraction::IsPointInPin(glm::vec2 point, const Pin& pin) {
		float distance = glm::length(point - pin.WorldPosition);
		return distance <= pin.Radius;
	}

	bool NodeEditorInteraction::IsPointOnConnection(glm::vec2 point, const Connection& connection) {
		const Pin* fromPin = m_Graph->FindPin(connection.FromPinID);
		const Pin* toPin = m_Graph->FindPin(connection.ToPinID);

		if (!fromPin || !toPin) return false;

		// Simple line distance check (can be improved with actual bezier curve)
		glm::vec2 start = fromPin->WorldPosition;
		glm::vec2 end = toPin->WorldPosition;

		// Project point onto line segment
		glm::vec2 lineVec = end - start;
		float lineLength = glm::length(lineVec);
		if (lineLength < 0.001f) return false;

		glm::vec2 lineDir = lineVec / lineLength;
		glm::vec2 pointVec = point - start;
		float projectionLength = glm::dot(pointVec, lineDir);

		// Clamp to line segment
		projectionLength = glm::clamp(projectionLength, 0.0f, lineLength);
		glm::vec2 closestPoint = start + lineDir * projectionLength;

		float distance = glm::length(point - closestPoint);
		return distance <= 5.0f; // 5 pixel tolerance
	}

	void NodeEditorInteraction::SelectNode(uint32_t nodeID, bool addToSelection) {
		if (!addToSelection) {
			m_SelectedNodes.clear();
		}

		auto it = std::find(m_SelectedNodes.begin(), m_SelectedNodes.end(), nodeID);
		if (it == m_SelectedNodes.end()) {
			m_SelectedNodes.push_back(nodeID);
		}
	}

	void NodeEditorInteraction::DeselectAll() {
		m_SelectedNodes.clear();
	}

	void NodeEditorInteraction::DeleteSelected() {
		for (uint32_t nodeID : m_SelectedNodes) {
			m_Graph->DeleteNode(nodeID);
		}

		m_SelectedNodes.clear();
	}

	void NodeEditorInteraction::UpdateBoxSelection() {
		glm::vec2 boxMin = glm::min(m_BoxSelectStart, m_BoxSelectEnd);
		glm::vec2 boxMax = glm::max(m_BoxSelectStart, m_BoxSelectEnd);

		m_SelectedNodes.clear();

		for (const auto& [nodeID, node] : m_Graph->GetNodes()) {
			// Check if the node overlaps with the selection box
			glm::vec2 nodeMin = node.Position;
			glm::vec2 nodeMax = node.Position + node.Size;

			bool overlaps = !(boxMax.x < nodeMin.x || boxMin.x > nodeMax.x ||
							  boxMax.y < nodeMin.y || boxMin.y > nodeMax.y);

			if (overlaps) {
				m_SelectedNodes.push_back(nodeID);
			}
		}
	}
}
