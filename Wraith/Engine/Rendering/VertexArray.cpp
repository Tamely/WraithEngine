#include "wpch.h"
#include "Rendering/VertexArray.h"

#include "Rendering/Renderer.h"
#include "OpenGL/OpenGLVertexArray.h"

namespace Wraith {
	Ref<VertexArray> VertexArray::Create() {
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:     W_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::OpenGL:   return std::make_shared<OpenGLVertexArray>();
		}

		W_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}
