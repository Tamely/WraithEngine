#include "wpch.h"
#include "BranchNode.h"

namespace Wraith {
	ImColor BranchNode::GetColor() const {
		return ImColor(200, 200, 200);
	}

	NodeType BranchNode::GetNodeType() const {
		return NodeType::Blueprint;
	}

	std::vector<std::pair<std::string, PinType>> BranchNode::GetInputs() const {
		return {
			{ "", PinType::Flow },
			{ "Condition", PinType::Bool }
		};
	}

	std::vector<std::pair<std::string, PinType>> BranchNode::GetOutputs() const {
		return {
			{ "True", PinType::Flow },
			{ "False", PinType::Flow }
		};
	}
}
