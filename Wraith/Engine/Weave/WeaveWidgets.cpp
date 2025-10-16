#include "wpch.h"
#include "WeaveWidgets.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>

namespace Wraith {
	void Icon(const ImVec2& size, Drawing::IconType type, bool filled, const ImVec4& color, const ImVec4& innerColor) {
		if (ImGui::IsRectVisible(size)) {
			ImVec2 cursorPos = ImGui::GetCursorScreenPos();
			ImDrawList* drawList = ImGui::GetWindowDrawList();

			Drawing::DrawIcon(drawList, cursorPos, cursorPos + size, type, filled, ImColor(color), ImColor(innerColor));
		}

		ImGui::Dummy(size);
	}
}
