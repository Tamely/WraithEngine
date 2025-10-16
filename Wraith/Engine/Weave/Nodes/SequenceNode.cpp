#include "wpch.h"
#include "SequenceNode.h"

namespace Wraith {
	ImColor SequenceNode::GetColor() const {
		return ImColor(200, 200, 200);
	}

	NodeType SequenceNode::GetNodeType() const {
		return NodeType::Tree;
	}

	std::vector<std::pair<std::string, PinType>> SequenceNode::GetInputs() const {
		return {
			{ "", PinType::Flow }
		};
	}

	std::vector<std::pair<std::string, PinType>> SequenceNode::GetOutputs() const {
		return {
			{ "", PinType::Flow }
		};
	}
}
