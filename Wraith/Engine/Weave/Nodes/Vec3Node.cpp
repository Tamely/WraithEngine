#include "wpch.h"
#include "Vec3Node.h"

namespace Wraith {
	ImColor Vec3Node::GetColor() const {
		return ImColor(100, 220, 150);
	}

	NodeType Vec3Node::GetNodeType() const {
		return NodeType::Blueprint;
	}

	std::vector<std::pair<std::string, PinType>> Vec3Node::GetInputs() const {
		return {};
	}

	std::vector<std::pair<std::string, PinType>> Vec3Node::GetOutputs() const {
		return {
			{ "Value", PinType::Vec3 }
		};
	}
}
