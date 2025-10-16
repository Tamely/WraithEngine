#pragma once
#include <Wraith.h>
#include <functional>

// Forward declare to avoid including imgui in a header file
using ImTextureID = void*;

namespace Wraith {
	enum class SceneState {
		Edit = 0,
		Play
	};

	class ToolbarPanel {
	public:
		ToolbarPanel();

		// Callbacks to notify base layer of state changes
		void SetPlayCallback(const std::function<void()>& callback) { m_PlayCallback = callback; }
		void SetStopCallback(const std::function<void()>& callback) { m_StopCallback = callback; }

		void OnImGuiRender(SceneState sceneState);

	private:
		std::function<void()> m_PlayCallback;
		std::function<void()> m_StopCallback;

		ImTextureID m_PlayIcon;
		ImTextureID m_StopIcon;
	};
}
