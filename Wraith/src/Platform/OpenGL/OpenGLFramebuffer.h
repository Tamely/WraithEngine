#pragma once

#include "Wraith/Renderer/Framebuffer.h"

namespace Wraith {
	class OpenGLFramebuffer : public Framebuffer {
	public:
		OpenGLFramebuffer(const FramebufferSpecification& spec);
		virtual ~OpenGLFramebuffer();

		void Invalidate();

		virtual void Bind() override;
		virtual void Unbind() override;

		virtual void Resize(uint32_t width, uint32_t height) override;

		virtual uint32_t GetColorAttachmentRendererID(uint32_t index = 0) const override { W_CORE_ASSERT(index < m_ColorAttachments.Size()); return m_ColorAttachments[index]; };

		virtual const FramebufferSpecification& GetSpecification() const override { return m_Specification; };
	private:
		uint32_t m_RendererID = 0;
		FramebufferSpecification m_Specification;

		Array<FramebufferTextureSpecification> m_ColorAttachmentSpecs;
		FramebufferTextureSpecification m_DepthAttachmentSpec = FramebufferTextureFormat::None;

		Array<uint32_t> m_ColorAttachments;
		uint32_t m_DepthAttachment;
	};
}
