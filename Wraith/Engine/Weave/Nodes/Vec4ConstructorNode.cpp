#include "wpch.h"
#include "Vec4ConstructorNode.h"

namespace Wraith {
	ImColor Vec4ConstructorNode::GetColor() const {
		return ImColor(120, 180, 255);
	}

	NodeType Vec4ConstructorNode::GetNodeType() const {
		return NodeType::Blueprint;
	}

	std::vector<std::pair<std::string, PinType>> Vec4ConstructorNode::GetInputs() const {
		return {
			{ "X", PinType::Float },
			{ "Y", PinType::Float },
			{ "Z", PinType::Float },
			{ "W", PinType::Float }
		};
	}

	std::vector<std::pair<std::string, PinType>> Vec4ConstructorNode::GetOutputs() const {
		return {
			{ "Result", PinType::Vec4 }
		};
	}
}
