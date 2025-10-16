#pragma once

#include "Weave/PinType.h"
#include "Weave/PinKind.h"

#include <ImGui-NodeEditor/imgui_node_editor.h>

#include <string>

namespace Wraith {
	struct Pin {
		ax::NodeEditor::PinId ID;
		struct Node* Node;
		std::string Name;
		PinType Type;
		PinKind Kind;

		Pin(int id, const char* name, PinType type)
			: ID(id), Node(nullptr), Name(name), Type(type), Kind(PinKind::Input) {}
	};
}
