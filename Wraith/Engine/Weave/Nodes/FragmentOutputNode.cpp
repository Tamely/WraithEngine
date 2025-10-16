#include "wpch.h"
#include "FragmentOutputNode.h"

namespace Wraith {
	ImColor FragmentOutputNode::GetColor() const {
		return ImColor(255, 100, 100);
	}

	NodeType FragmentOutputNode::GetNodeType() const {
		return NodeType::Blueprint;
	}

	std::vector<std::pair<std::string, PinType>> FragmentOutputNode::GetInputs() const {
		return {
			{ "", PinType::Flow },
			{ "Color", PinType::Vec4 }
		};
	}

	std::vector<std::pair<std::string, PinType>> FragmentOutputNode::GetOutputs() const {
		return {};
	}
}
