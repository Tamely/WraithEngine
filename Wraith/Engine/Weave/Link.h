#pragma once

#include <imgui_internal.h>
#include <ImGui-NodeEditor/imgui_node_editor.h>

namespace Wraith {
	struct Link {
		ax::NodeEditor::LinkId ID;

		ax::NodeEditor::PinId StartPinID;
		ax::NodeEditor::PinId EndPinID;

		ImColor Color;

		Link(ax::NodeEditor::LinkId id, ax::NodeEditor::PinId startPinId, ax::NodeEditor::PinId endPinId)
			: ID(id), StartPinID(startPinId), EndPinID(endPinId), Color(255, 255, 255) {}
	};
}
