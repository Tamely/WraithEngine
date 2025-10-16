#include "wpch.h"
#include "Vec3ConstructorNode.h"

namespace Wraith {
	ImColor Vec3ConstructorNode::GetColor() const {
		return ImColor(120, 180, 255);
	}

	NodeType Vec3ConstructorNode::GetNodeType() const {
		return NodeType::Blueprint;
	}

	std::vector<std::pair<std::string, PinType>> Vec3ConstructorNode::GetInputs() const {
		return {
			{ "X", PinType::Float },
			{ "Y", PinType::Float },
			{ "Z", PinType::Float }
		};
	}

	std::vector<std::pair<std::string, PinType>> Vec3ConstructorNode::GetOutputs() const {
		return {
			{ "Result", PinType::Vec3 }
		};
	}
}
