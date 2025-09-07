#include "wpch.h"
#include "UI/UIRenderCommand.h"
#include "UI/ImGui/ImGuiRenderer.h"

namespace Wraith {
	Scope<UIRendererAPI> UIRenderCommand::s_RendererAPI = CreateScope<ImGuiRenderer>();
}
