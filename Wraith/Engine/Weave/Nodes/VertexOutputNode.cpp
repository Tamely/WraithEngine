#include "wpch.h"
#include "VertexOutputNode.h"

namespace Wraith {
	ImColor VertexOutputNode::GetColor() const {
		return ImColor(255, 100, 100);
	}

	NodeType VertexOutputNode::GetNodeType() const {
		return NodeType::Blueprint;
	}

	std::vector<std::pair<std::string, PinType>> VertexOutputNode::GetInputs() const {
		return {
			{ "", PinType::Flow },
			{ "gl_Position", PinType::Vec4 }
		};
	}

	std::vector<std::pair<std::string, PinType>> VertexOutputNode::GetOutputs() const {
		return {};
	}
}
