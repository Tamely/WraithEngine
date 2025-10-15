#pragma once

#include "Core/NodeGraph.h"
#include "Rendering/NodeEditorCamera.h"

#include <glm/glm.hpp>
#include <vector>

namespace Wraith {
	class NodeEditorInteraction {
	public:
		enum class State {
			None,
			DraggingNode,
			DraggingSelection,
			ConnectingPins,
			Panning,
			BoxSelecting
		};

	public:
		NodeEditorInteraction(NodeGraph* graph, NodeEditorCamera* camera, class NodeEditorRenderer* renderer)
			: m_Graph(graph), m_Camera(camera), m_Renderer(renderer) {}

		void OnMouseButtonPressed(glm::vec2 mousePos, int button);
		void OnMouseButtonReleased(glm::vec2 mousePos, int button);
		void OnMouseMoved(glm::vec2 mousePos);
		void OnScroll(float delta);
		void OnKeyPressed(int key);

		void Update();

	public:
		State GetState() const { return m_State; }
		const std::vector<uint32_t>& GetSelectedNodes() const { return m_SelectedNodes; }
		uint32_t GetHoveredNode() const { return m_HoveredNodeID; }
		uint32_t GetHoveredPin() const { return m_HoveredPinID; }
		bool ShouldShowConnectionPreview() const { return m_ShowConnectionPreview; }
		glm::vec2 GetConnectionPreviewEnd() const { return m_ConnectionPreviewEnd; }
		glm::vec2 GetBoxSelectStart() const { return m_BoxSelectStart; }
		glm::vec2 GetBoxSelectEnd() const { return m_BoxSelectEnd; }

	private:
		uint32_t GetNodeUnderMouse(glm::vec2 worldPos);
		uint32_t GetPinUnderMouse(glm::vec2 worldPos);
		uint32_t GetConnectionUnderMouse(glm::vec2 worldPos);
		bool IsPointInNode(glm::vec2 point, const Node& node);
		bool IsPointInPin(glm::vec2 point, const Pin& pin);
		bool IsPointOnConnection(glm::vec2 point, const Connection& connection);

		void SelectNode(uint32_t nodeID, bool addToSelection = false);
		void DeselectAll();
		void DeleteSelected();
		void UpdateBoxSelection();

	private:
		NodeGraph* m_Graph;
		NodeEditorCamera* m_Camera;
		class NodeEditorRenderer* m_Renderer;

		State m_State = State::None;

		uint32_t m_HoveredNodeID = 0;
		uint32_t m_HoveredPinID = 0;
		uint32_t m_DragStartPinID = 0;
		glm::vec2 m_LastMousePos;
		glm::vec2 m_DragStartPos;
		std::vector<uint32_t> m_SelectedNodes;

		// Box selection
		glm::vec2 m_BoxSelectStart;
		glm::vec2 m_BoxSelectEnd;

		// Connection preview
		bool m_ShowConnectionPreview = false;
		glm::vec2 m_ConnectionPreviewEnd;
	};
}
