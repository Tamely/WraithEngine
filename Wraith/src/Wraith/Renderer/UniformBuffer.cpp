#include "wpch.h"
#include "UniformBuffer.h"

#include "Wraith/Renderer/Renderer.h"
#include "Platform/OpenGL/OpenGLUniformBuffer.h"

namespace Wraith {
	Ref<UniformBuffer> UniformBuffer::Create(uint32_t size, uint32_t binding) {
		switch (Renderer::GetAPI()) {
			case RendererAPI::API::None: W_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::OpenGL: return CreateRef<OpenGLUniformBuffer>(size, binding);
		}

		W_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}
