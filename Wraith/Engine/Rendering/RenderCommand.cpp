#include "wpch.h"
#include "Rendering/RenderCommand.h"

#include "OpenGL/OpenGLRendererAPI.h"

namespace Wraith {
	Scope<RendererAPI> RenderCommand::s_RendererAPI = CreateScope<OpenGLRendererAPI>();
}
