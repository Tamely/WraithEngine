#pragma once
#include <glm/glm.hpp>

#include <cstdarg>

namespace Wraith {
	class UIRendererAPI {
	public:
		enum class API {
			None = 0, ImGui = 1
		};
	public:
		virtual ~UIRendererAPI() = default;
		virtual void Init() = 0;

		virtual bool Begin(const char* name, bool* open = nullptr, int flags = 0) = 0;
		virtual void End() = 0;
		virtual bool Button(const char* label, glm::vec2 size = { 0,0 }) = 0;
		virtual void Text(const char* fmt, ...) = 0;
		virtual void TextV(const char* fmt, va_list args) = 0;  // For variadic forwarding
		virtual bool TreeNode(const char* label) = 0;
		virtual void TreePop() = 0;
		virtual bool InputText(const char* label, char* buf, size_t bufSize) = 0;
		virtual void Image(void* textureId, glm::vec2 size) = 0;
		virtual void Separator() = 0;
		virtual bool BeginMenuBar() = 0;
		virtual void EndMenuBar() = 0;
		virtual bool BeginMenu(const char* label) = 0;
		virtual void EndMenu() = 0;
		virtual bool MenuItem(const char* label, const char* shortcut = nullptr) = 0;
		virtual bool Checkbox(const char* label, bool* v) = 0;
		virtual bool BeginCombo(const char* label, const char* previewValue, int flags = 0) = 0;
		virtual void EndCombo() = 0;
		virtual bool Selectable(const char* label, bool selected = false, int flags = 0, const glm::vec2& size = glm::vec2{ 0, 0 }) = 0;
		virtual void SetItemDefaultFocus() = 0;
		virtual bool DragFloat(const char* label, float* v, float vSpeed = 1.0f, float vMin = 0.0f, float vMax = 0.0f, const char* format = "%.3f", int flags = 0) = 0;
		virtual void Vec2Control(const std::string& label, glm::vec2& values, float resetValue = 0.0f, float columnWidth = 100.0f) = 0;
		virtual void Vec3Control(const std::string& label, glm::vec3& values, float resetValue = 0.0f, float columnWidth = 100.0f) = 0;
		virtual bool ColorEdit4(const char* label, float col[4], int flags = 0) = 0;
		virtual bool BeginDragDropTarget() = 0;
		virtual const struct UIDragDropPayload* AcceptDragDropPayload(const char* type, int flags = 0) = 0;
		virtual void EndDragDropTarget() = 0;

		inline static API GetAPI() { return s_API; }
	private:
		static API s_API;
	};
}
