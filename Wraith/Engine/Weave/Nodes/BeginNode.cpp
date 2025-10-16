#include "wpch.h"
#include "BeginNode.h"

namespace Wraith {
	ImColor BeginNode::GetColor() const {
		return ImColor(180, 120, 255);
	}

	NodeType BeginNode::GetNodeType() const {
		return NodeType::Blueprint;
	}

	std::vector<std::pair<std::string, PinType>> BeginNode::GetInputs() const {
		return {};
	}

	std::vector<std::pair<std::string, PinType>> BeginNode::GetOutputs() const {
		return {
			{ "Start", PinType::Flow }
		};
	}
}
