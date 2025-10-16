#include "wpch.h"
#include "Vec4Node.h"

namespace Wraith {
	ImColor Vec4Node::GetColor() const {
		return ImColor(200, 200, 200);
	}

	NodeType Vec4Node::GetNodeType() const {
		return NodeType::Blueprint;
	}

	std::vector<std::pair<std::string, PinType>> Vec4Node::GetInputs() const {
		return {};
	}

	std::vector<std::pair<std::string, PinType>> Vec4Node::GetOutputs() const {
		return {
			{ "Value", PinType::Vec4 }
		};
	}
}
