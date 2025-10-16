#pragma once

#include "Weave/PinType.h"
#include "Weave/NodeType.h"

#include <string>
#include <vector>

#include <imgui.h>

namespace Wraith {
	class INodeDefinition {
	public:
		virtual ~INodeDefinition() = default;
		virtual std::string GetName() const = 0;
		virtual std::string GetCategory() const = 0;
		virtual ImColor GetColor() const = 0;
		virtual NodeType GetNodeType() const = 0;
		virtual std::vector<std::pair<std::string, PinType>> GetInputs() const = 0;
		virtual std::vector<std::pair<std::string, PinType>> GetOutputs() const = 0;
		virtual ImVec2 GetDefaultSize() const = 0;
	};
}
