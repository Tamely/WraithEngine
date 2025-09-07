#pragma once
#include "Core/CoreBasic.h"
#include "UI/UIRendererAPI.h"

#include <glm/glm.hpp>

namespace Wraith {
	class UIRenderCommand {
	public:
		inline static void Init() {
			s_RendererAPI->Init();
		}

		inline static bool Begin(const char* name, bool* open = nullptr, int flags = 0) {
			return s_RendererAPI->Begin(name, open, flags);
		}

		inline static void End() {
			s_RendererAPI->End();
		}

		inline static bool Button(const char* label, glm::vec2 size = { 0,0 }) {
			return s_RendererAPI->Button(label, size);
		}

		inline static void Text(const char* fmt, ...) {
			va_list args;
			va_start(args, fmt);
			s_RendererAPI->TextV(fmt, args);
			va_end(args);
		}

		inline static bool TreeNode(const char* label) {
			return s_RendererAPI->TreeNode(label);
		}

		inline static void TreePop() {
			s_RendererAPI->TreePop();
		}

		inline static bool InputText(const char* label, char* buf, size_t bufSize) {
			return s_RendererAPI->InputText(label, buf, bufSize);
		}

		inline static void Image(void* textureId, glm::vec2 size) {
			s_RendererAPI->Image(textureId, size);
		}

		inline static void Separator() {
			s_RendererAPI->Separator();
		}

		inline static bool BeginMenuBar() {
			return s_RendererAPI->BeginMenuBar();
		}

		inline static void EndMenuBar() {
			s_RendererAPI->EndMenuBar();
		}

		inline static bool BeginMenu(const char* label) {
			return s_RendererAPI->BeginMenu(label);
		}

		inline static void EndMenu() {
			s_RendererAPI->EndMenu();
		}

		inline static bool MenuItem(const char* label, const char* shortcut = nullptr) {
			return s_RendererAPI->MenuItem(label, shortcut);
		}

		inline static bool Checkbox(const char* label, bool* v) {
			return s_RendererAPI->Checkbox(label, v);
		}

		inline static bool BeginCombo(const char* label, const char* previewValue, int flags = 0) {
			return s_RendererAPI->BeginCombo(label, previewValue, flags);
		}

		inline static void EndCombo() {
			s_RendererAPI->EndCombo();
		}

		inline static bool Selectable(const char* label, bool selected = false, int flags = 0, const glm::vec2& size = glm::vec2{ 0, 0 }) {
			return s_RendererAPI->Selectable(label, selected, flags, size);
		}

		inline static void SetItemDefaultFocus() {
			s_RendererAPI->SetItemDefaultFocus();
		}

		inline static bool DragFloat(const char* label, float* v, float vSpeed = 1.0f, float vMin = 0.0f, float vMax = 0.0f, const char* format = "%.3f", int flags = 0) {
			return s_RendererAPI->DragFloat(label, v, vSpeed, vMin, vMax, format, flags);
		}

		inline static void Vec2Control(const std::string& label, glm::vec2& values, float resetValue = 0.0f, float columnWidth = 100.0f) {
			return s_RendererAPI->Vec2Control(label, values, resetValue, columnWidth);
		}

		inline static void Vec3Control(const std::string& label, glm::vec3& values, float resetValue = 0.0f, float columnWidth = 100.0f) {
			return s_RendererAPI->Vec3Control(label, values, resetValue, columnWidth);
		}

		inline static bool ColorEdit4(const char* label, float col[4], int flags = 0) {
			return s_RendererAPI->ColorEdit4(label, col, flags);
		}

		inline static bool BeginDragDropTarget() {
			return s_RendererAPI->BeginDragDropTarget();
		}

		inline static const struct UIDragDropPayload* AcceptDragDropPayload(const char* type, int flags = 0) {
			return s_RendererAPI->AcceptDragDropPayload(type, flags);
		}

		inline static void EndDragDropTarget() {
			return s_RendererAPI->EndDragDropTarget();
		}

	private:
		static Scope<UIRendererAPI> s_RendererAPI;
	};
}
