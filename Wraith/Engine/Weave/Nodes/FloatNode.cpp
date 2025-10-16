#include "wpch.h"
#include "FloatNode.h"

namespace Wraith {
	ImColor FloatNode::GetColor() const {
		return ImColor(200, 200, 200);
	}

	NodeType FloatNode::GetNodeType() const {
		return NodeType::Blueprint;
	}

	std::vector<std::pair<std::string, PinType>> FloatNode::GetInputs() const {
		return {};
	}

	std::vector<std::pair<std::string, PinType>> FloatNode::GetOutputs() const {
		return {
			{ "Value", PinType::Float }
		};
	}
}
