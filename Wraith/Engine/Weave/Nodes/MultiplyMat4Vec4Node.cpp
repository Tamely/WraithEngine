#include "wpch.h"
#include "MultiplyMat4Vec4Node.h"

namespace Wraith {
	ImColor MultiplyMat4Vec4Node::GetColor() const {
		return ImColor(120, 180, 255);
	}

	NodeType MultiplyMat4Vec4Node::GetNodeType() const {
		return NodeType::Blueprint;
	}

	std::vector<std::pair<std::string, PinType>> MultiplyMat4Vec4Node::GetInputs() const {
		return {
			{ "A", PinType::Mat4 },
			{ "B", PinType::Vec4 }
		};
	}

	std::vector<std::pair<std::string, PinType>> MultiplyMat4Vec4Node::GetOutputs() const {
		return {
			{ "Result", PinType::Vec4 }
		};
	}
}
