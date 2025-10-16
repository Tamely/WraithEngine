#pragma once

#include "Weave/Pin.h"
#include "Weave/NodeType.h"

#include <imgui_internal.h>
#include <ImGui-NodeEditor/imgui_node_editor.h>

#include <string>
#include <vector>

namespace Wraith {
	struct Node {
		ax::NodeEditor::NodeId ID;
		std::string Name;
		std::vector<Pin> Inputs;
		std::vector<Pin> Outputs;
		ImColor Color;
		NodeType Type;
		ImVec2 Size;

		std::string State;
		std::string SavedState;

		Node(int id, const char* name, ImColor color = ImColor(255, 255, 255, 255))
			: ID(id), Name(name), Color(color), Type(NodeType::Blueprint), Size(0, 0) {}
	};
}
