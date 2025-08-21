#include "wpch.h"
#include "Rendering/Framebuffer.h"

#include "Rendering/Renderer.h"

#include "OpenGL/OpenGLFramebuffer.h"

namespace Wraith {
	Ref<Framebuffer> Framebuffer::Create(const FramebufferSpecification& spec) {
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:     W_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::OpenGL:   return std::make_shared<OpenGLFramebuffer>(spec);
		}

		W_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}
