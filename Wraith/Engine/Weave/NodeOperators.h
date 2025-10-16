#pragma once

#include <ImGui-NodeEditor/imgui_node_editor.h>

struct NodeIdLess {
	bool operator()(const ax::NodeEditor::NodeId& lhs, const ax::NodeEditor::NodeId& rhs) const {
		return lhs.AsPointer() < rhs.AsPointer();
	}
};
