#include "wpch.h"
#include "RenderCommand.h"

#include "Platform/OpenGL/OpenGLRendererAPI.h"

namespace Wraith {
	RendererAPI* RenderCommand::s_RendererAPI = new OpenGLRendererAPI;
}