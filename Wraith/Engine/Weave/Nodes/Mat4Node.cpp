#include "wpch.h"
#include "Mat4Node.h"

namespace Wraith {
	ImColor Mat4Node::GetColor() const {
		return ImColor(200, 200, 200);
	}

	NodeType Mat4Node::GetNodeType() const {
		return NodeType::Blueprint;
	}

	std::vector<std::pair<std::string, PinType>> Mat4Node::GetInputs() const {
		return {};
	}

	std::vector<std::pair<std::string, PinType>> Mat4Node::GetOutputs() const {
		return {
			{ "Value", PinType::Mat4 }
		};
	}
}
