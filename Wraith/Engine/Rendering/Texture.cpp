#include "wpch.h"
#include "Rendering/Texture.h"

#include "Rendering/Renderer.h"
#include "OpenGL/OpenGLTexture.h"

namespace Wraith {
	Ref<Texture2D> Texture2D::Create(uint32_t width, uint32_t height) {
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:     W_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::OpenGL:   return CreateRef<OpenGLTexture2D>(width, height);
		}

		W_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

	Ref<Texture2D> Texture2D::Create(const std::string& path, bool flipVertically) {
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:     W_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::OpenGL:   return CreateRef<OpenGLTexture2D>(path, flipVertically);
		}

		W_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}
