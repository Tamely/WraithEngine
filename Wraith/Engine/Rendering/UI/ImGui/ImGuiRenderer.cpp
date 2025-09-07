#include "wpch.h"
#include "ImGuiRenderer.h"
#include "UI/UIDragDropPayload.h"

#include <ImGui-Premake/imgui.h>
#include <ImGui-Premake/imgui_internal.h>

namespace Wraith {
	void ImGuiRenderer::Init() {
		// TODO: Move ImGui initialization here
	}

	bool ImGuiRenderer::Begin(const char* name, bool* open, int flags) {
		return ImGui::Begin(name, open, flags);
	}

	bool ImGuiRenderer::DragFloat(const char* label, float* v, float vSpeed, float vMin, float vMax, const char* format, int flags) {
		ImGui::PushID(label);

		ImGui::Columns(2);
		float textWidth = ImGui::CalcTextSize(label).x + ImGui::GetStyle().ItemSpacing.x * 2.0f;
		float columnWidth = std::max(100.0f, textWidth);
		ImGui::SetColumnWidth(0, columnWidth);
		ImGui::Text(label);
		ImGui::NextColumn();

		bool modified = ImGui::DragFloat("##DragFloat", v, vSpeed, vMin, vMax, format, flags);

		ImGui::Columns(1);
		ImGui::PopID();

		return modified;
	}

	void ImGuiRenderer::End() {
		ImGui::End();
	}

	bool ImGuiRenderer::Button(const char* label, glm::vec2 size) {
		return ImGui::Button(label, ImVec2(size.x, size.y));
	}

	void ImGuiRenderer::Text(const char* fmt, ...) {
		va_list args;
		va_start(args, fmt);
		ImGui::TextV(fmt, args); 
		va_end(args);
	}

	void ImGuiRenderer::TextV(const char* fmt, va_list args) {
		ImGui::TextV(fmt, args);
	}

	bool ImGuiRenderer::TreeNode(const char* label) {
		return ImGui::TreeNode(label);
	}

	void ImGuiRenderer::TreePop() {
		ImGui::TreePop();
	}

	bool ImGuiRenderer::InputText(const char* label, char* buf, size_t bufSize) {
		return ImGui::InputText(label, buf, bufSize);
	}

	void ImGuiRenderer::Image(void* textureId, glm::vec2 size) {
		ImGui::Image(textureId, ImVec2(size.x, size.y));
	}

	void ImGuiRenderer::Separator() {
		ImGui::Separator();
	}

	bool ImGuiRenderer::BeginMenuBar() {
		return ImGui::BeginMenuBar();
	}

	void ImGuiRenderer::EndMenuBar() {
		ImGui::EndMenuBar();
	}

	bool ImGuiRenderer::BeginMenu(const char* label) {
		return ImGui::BeginMenu(label);
	}

	void ImGuiRenderer::EndMenu() {
		ImGui::EndMenu();
	}

	bool ImGuiRenderer::MenuItem(const char* label, const char* shortcut) {
		return ImGui::MenuItem(label, shortcut);
	}

	bool ImGuiRenderer::Checkbox(const char* label, bool* v) {
		ImGui::PushID(label);

		ImGui::Columns(2);
		float textWidth = ImGui::CalcTextSize(label).x + ImGui::GetStyle().ItemSpacing.x * 2.0f;
		float columnWidth = std::max(100.0f, textWidth);
		ImGui::SetColumnWidth(0, columnWidth);
		ImGui::Text(label);
		ImGui::NextColumn();

		bool modified = ImGui::Checkbox("##Checkbox", v);

		ImGui::Columns(1);
		ImGui::PopID();

		return modified;
	}

	bool ImGuiRenderer::BeginCombo(const char* label, const char* previewValue, int flags) {
		ImGui::PushID(label);

		ImGui::Columns(2);
		float textWidth = ImGui::CalcTextSize(label).x + ImGui::GetStyle().ItemSpacing.x * 2.0f;
		float columnWidth = std::max(100.0f, textWidth);
		ImGui::SetColumnWidth(0, columnWidth);
		ImGui::Text(label);
		ImGui::NextColumn();

		bool result = ImGui::BeginCombo("##Combo", previewValue, flags);

		if (!result) {
			ImGui::Columns(1);
			ImGui::PopID();
		}

		return result;
	}

	void ImGuiRenderer::EndCombo() {
		ImGui::EndCombo();
		ImGui::Columns(1);
		ImGui::PopID();
	}

	bool ImGuiRenderer::Selectable(const char* label, bool selected, int flags, const glm::vec2& size) {
		return ImGui::Selectable(label, selected, flags, ImVec2(size.x, size.y));
	}

	void ImGuiRenderer::SetItemDefaultFocus() {
		ImGui::SetItemDefaultFocus();
	}

	void ImGuiRenderer::Vec2Control(const std::string& label, glm::vec2& values, float resetValue, float columnWidth) {
		ImGuiIO& io = ImGui::GetIO();
		ImFont* boldFont = io.Fonts->Fonts[0];

		ImGui::PushID(label.c_str());

		ImGui::Columns(2);

		ImGui::SetColumnWidth(0, columnWidth);
		ImGui::Text(label.c_str());
		ImGui::NextColumn();

		ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 0, 0 });

		float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
		ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("X", buttonSize)) values.x = resetValue;
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("Y", buttonSize)) values.y = resetValue;
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();

		ImGui::PopStyleVar(2);

		ImGui::Columns(1);

		ImGui::PopID();
	}

	void ImGuiRenderer::Vec3Control(const std::string& label, glm::vec3& values, float resetValue, float columnWidth) {
		ImGuiIO& io = ImGui::GetIO();
		ImFont* boldFont = io.Fonts->Fonts[0];

		ImGui::PushID(label.c_str());

		ImGui::Columns(2);

		ImGui::SetColumnWidth(0, columnWidth);
		ImGui::Text(label.c_str());
		ImGui::NextColumn();

		ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 0, 0 });

		float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
		ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("X", buttonSize)) values.x = resetValue;
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("Y", buttonSize)) values.y = resetValue;
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("Z", buttonSize)) values.z = resetValue;
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();

		ImGui::PopStyleVar();

		ImGui::Columns(1);

		ImGui::PopID();
	}

	bool ImGuiRenderer::ColorEdit4(const char* label, float col[4], int flags) {
		ImGuiIO& io = ImGui::GetIO();
		ImFont* boldFont = io.Fonts->Fonts[0];

		ImGui::PushID(label);

		ImGui::Columns(2);
		float textWidth = ImGui::CalcTextSize(label).x + ImGui::GetStyle().ItemSpacing.x * 2.0f;
		float columnWidth = std::max(100.0f, textWidth);
		ImGui::SetColumnWidth(0, columnWidth);
		ImGui::Text(label);
		ImGui::NextColumn();

		ImGui::PushMultiItemsWidths(4, ImGui::CalcItemWidth());
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 0, 0 });

		float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
		ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

		bool modified = false;

		// R
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("R", buttonSize)) { col[0] = 0.0f; modified = true; }
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		if (ImGui::DragFloat("##R", &col[0], 0.01f, 0.0f, 1.0f, "%.2f")) modified = true;
		ImGui::PopItemWidth();
		ImGui::SameLine();

		// G
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("G", buttonSize)) { col[1] = 0.0f; modified = true; }
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		if (ImGui::DragFloat("##G", &col[1], 0.01f, 0.0f, 1.0f, "%.2f")) modified = true;
		ImGui::PopItemWidth();
		ImGui::SameLine();

		// B
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("B", buttonSize)) { col[2] = 0.0f; modified = true; }
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		if (ImGui::DragFloat("##B", &col[2], 0.01f, 0.0f, 1.0f, "%.2f")) modified = true;
		ImGui::PopItemWidth();
		ImGui::SameLine();

		// A
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.5f, 0.5f, 0.5f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.6f, 0.6f, 0.6f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.5f, 0.5f, 0.5f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("A", buttonSize)) { col[3] = 1.0f; modified = true; }
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		if (ImGui::DragFloat("##A", &col[3], 0.01f, 0.0f, 1.0f, "%.2f")) modified = true;
		ImGui::PopItemWidth();

		// Color preview button
		ImGui::SameLine();
		ImVec2 colorButtonSize = { lineHeight * 2.0f, lineHeight };
		if (ImGui::ColorButton("##ColorPreview", ImVec4(col[0], col[1], col[2], col[3]), ImGuiColorEditFlags_NoTooltip, colorButtonSize)) {
			ImGui::OpenPopup("ColorPicker");
		}

		// Color picker popup
		if (ImGui::BeginPopup("ColorPicker")) {
			if (ImGui::ColorPicker4("##picker", col, ImGuiColorEditFlags_AlphaBar)) {
				modified = true;
			}
			ImGui::EndPopup();
		}

		ImGui::PopStyleVar(2);

		ImGui::Columns(1);

		ImGui::PopID();

		return modified;
	}

	bool ImGuiRenderer::BeginDragDropTarget() {
		return ImGui::BeginDragDropTarget();
	}

	const UIDragDropPayload* ImGuiRenderer::AcceptDragDropPayload(const char* type, int flags) {
		return reinterpret_cast<const UIDragDropPayload*>(ImGui::AcceptDragDropPayload(type, flags));
	}

	void ImGuiRenderer::EndDragDropTarget() {
		ImGui::EndDragDropTarget();
	}
}
