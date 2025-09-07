#pragma once
#include "UI/UIRendererAPI.h"

#include <glm/glm.hpp>
#include <cstdarg>

namespace Wraith {
	class ImGuiRenderer : public UIRendererAPI {
	public:
		virtual ~ImGuiRenderer() = default;
		virtual void Init() override;
		virtual bool Begin(const char* name, bool* open = nullptr, int flags = 0) override;
		virtual void End() override;
		virtual bool Button(const char* label, glm::vec2 size = { 0,0 }) override;
		virtual void Text(const char* fmt, ...) override;
		virtual void TextV(const char* fmt, va_list args) override;
		virtual bool TreeNode(const char* label) override;
		virtual void TreePop() override;
		virtual bool InputText(const char* label, char* buf, size_t bufSize) override;
		virtual void Image(void* textureId, glm::vec2 size) override;
		virtual void Separator() override;
		virtual bool BeginMenuBar() override;
		virtual void EndMenuBar() override;
		virtual bool BeginMenu(const char* label) override;
		virtual void EndMenu() override;
		virtual bool MenuItem(const char* label, const char* shortcut = nullptr) override;
		virtual bool Checkbox(const char* label, bool* v) override;
		virtual bool BeginCombo(const char* label, const char* previewValue, int flags = 0) override;
		virtual void EndCombo() override;
		virtual bool Selectable(const char* label, bool selected = false, int flags = 0, const glm::vec2& size = glm::vec2{ 0, 0 }) override;
		virtual void SetItemDefaultFocus() override;
		virtual bool DragFloat(const char* label, float* v, float vSpeed = 1.0f, float vMin = 0.0f, float vMax = 0.0f, const char* format = "%.3f", int flags = 0) override;
		virtual void Vec2Control(const std::string& label, glm::vec2& values, float resetValue = 0.0f, float columnWidth = 100.0f) override;
		virtual void Vec3Control(const std::string& label, glm::vec3& values, float resetValue = 0.0f, float columnWidth = 100.0f) override;
		virtual bool ColorEdit4(const char* label, float col[4], int flags = 0) override;
		virtual bool BeginDragDropTarget() override;
		virtual const struct UIDragDropPayload* AcceptDragDropPayload(const char* type, int flags = 0) override;
		virtual void EndDragDropTarget() override;
	};
}
