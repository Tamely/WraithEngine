#include "wpch.h"
#include "OpenGLContext.h"

#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <gl/GL.h>

namespace Wraith {
	OpenGLContext::OpenGLContext(GLFWwindow* windowHandle)
		: m_WindowHandle(windowHandle) {
		W_CORE_ASSERT(windowHandle, "Window handle is null!");
	}

	void OpenGLContext::Init() {
		W_PROFILE_FUNCTION();

		glfwMakeContextCurrent(m_WindowHandle);
		int status = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
		W_CORE_ASSERT(status, "Failed to initialize Glad!");

		W_CORE_INFO("OpenGL Info:");
		W_CORE_INFO("   Vendor: {0}", glGetString(GL_VENDOR));
		W_CORE_INFO("   Renderer: {0}", glGetString(GL_RENDERER));
		W_CORE_INFO("   Version: {0}", glGetString(GL_VERSION));
	}

	void OpenGLContext::SwapBuffers() {
		W_PROFILE_FUNCTION();

		glfwSwapBuffers(m_WindowHandle);
	}
}