#include "wpch.h"
#include "AddNode.h"

namespace Wraith {
	ImColor AddNode::GetColor() const {
		return ImColor(120, 180, 255);
	}

	NodeType AddNode::GetNodeType() const {
		return NodeType::Simple;
	}

	std::vector<std::pair<std::string, PinType>> AddNode::GetInputs() const {
		return {
			{ "A", PinType::Float },
			{ "B", PinType::Float }
		};
	}

	std::vector<std::pair<std::string, PinType>> AddNode::GetOutputs() const {
		return {
			{ "Result", PinType::Float }
		};
	}
}
