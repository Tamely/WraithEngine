#pragma once

#include <Weave/Node.h>
#include <Weave/Pin.h>
#include <Weave/Link.h>
#include <Weave/NodeOperators.h>

#include <ImGui-NodeEditor/imgui_node_editor.h>

#include <map>
#include <vector>

using ImTextureID = void*;

namespace Wraith {
	class WeavePanel {
	public:
		WeavePanel();
		~WeavePanel();

		void OnImGuiRender();
		void OnUpdate(float deltaTime);

	private:
		Node* SpawnNodeFromRegistry(const std::string& definitionName);
		void RenderContextMenu(ImVec2 openPopupPosition, Pin* newNodeLinkPin, bool& createNewNode);

		void BuildNode(Node* node);
		Node* FindNode(ax::NodeEditor::NodeId id);
		void TouchNode(ax::NodeEditor::NodeId id);

		bool CanCreateLink(Pin* a, Pin* b);
		Link* FindLink(ax::NodeEditor::LinkId id);

		Pin* FindPin(ax::NodeEditor::PinId id);
		bool IsPinLinked(ax::NodeEditor::PinId id);

		ImColor GetIconColor(PinType type);
		void DrawPinIcon(const Pin& pin, bool connected, int alpha);

		float GetTouchProgress(ax::NodeEditor::NodeId id);
		void UpdateTouch();

		int GetNextId();
		ax::NodeEditor::LinkId GetNextLinkId();

	private:
		ax::NodeEditor::EditorContext* m_EditorContext = nullptr;
		int m_NextId = 1;
		std::vector<Node> m_Nodes;
		std::vector<Link> m_Links;
		ImTextureID m_HeaderBackground = nullptr;
		const int m_PinIconSize = 24;
		const float m_TouchTime = 1.0f;
		std::map<ax::NodeEditor::NodeId, float, NodeIdLess> m_NodeTouchTime;
	};
}
