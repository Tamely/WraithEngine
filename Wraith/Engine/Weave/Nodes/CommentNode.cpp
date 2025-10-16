#include "wpch.h"
#include "CommentNode.h"

namespace Wraith {
	ImColor CommentNode::GetColor() const {
		return ImColor(200, 200, 200);
	}

	NodeType CommentNode::GetNodeType() const {
		return NodeType::Comment;
	}

	std::vector<std::pair<std::string, PinType>> CommentNode::GetInputs() const {
		return {};
	}

	std::vector<std::pair<std::string, PinType>> CommentNode::GetOutputs() const {
		return {};
	}
}
