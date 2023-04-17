#include "wpch.h"
#include "Renderer2D.h"

#include "VertexArray.h"
#include "Shader.h"
#include "RenderCommand.h"

#include "Platform/OpenGL/OpenGLShader.h"

namespace Wraith {

	struct Renderer2DStorage {
		Ref<VertexArray> QuadVertexArray;
		Ref<Shader> FlatColorShader;
	};

	static Renderer2DStorage* s_Data;

	void Renderer2D::Init() {
		s_Data = new Renderer2DStorage();
		s_Data->QuadVertexArray = Wraith::VertexArray::Create();

		// X Y Z
		float squareVertices[3 * 4] = {
			-0.5f, -0.5f, 0.0f,
			 0.5f, -0.5f, 0.0f,
			 0.5f,  0.5f, 0.0f,
			-0.5f,  0.5f, 0.0f,
		};

		Wraith::Ref<Wraith::VertexBuffer> squareVB;
		squareVB.reset(Wraith::VertexBuffer::Create(squareVertices, sizeof(squareVertices)));
		squareVB->SetLayout({
			{ Wraith::ShaderDataType::Float3, "a_Position" },
		});
		s_Data->QuadVertexArray->AddVertexBuffer(squareVB);

		uint32_t squareIndices[6] = { 0, 1, 2, 2, 3, 0 };
		Wraith::Ref<Wraith::IndexBuffer> squareIB;
		squareIB.reset(Wraith::IndexBuffer::Create(squareIndices, sizeof(squareIndices) / sizeof(uint32_t)));
		s_Data->QuadVertexArray->SetIndexBuffer(squareIB);

		s_Data->FlatColorShader = Wraith::Shader::Create("assets/shaders/FlatColor.glsl");
	}

	void Renderer2D::Shutdown() {
		delete s_Data;
	}

	void Renderer2D::BeginScene(const OrthographicCamera& camera) {
		std::dynamic_pointer_cast<Wraith::OpenGLShader>(s_Data->FlatColorShader)->Bind();
		std::dynamic_pointer_cast<Wraith::OpenGLShader>(s_Data->FlatColorShader)->UploadUniformMat4("u_ViewProjection", camera.GetViewProjectionMatrix());
		std::dynamic_pointer_cast<Wraith::OpenGLShader>(s_Data->FlatColorShader)->UploadUniformMat4("u_Transform", glm::mat4(1.0f));
	}

	void Renderer2D::EndScene() {
	}

	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color) {
		DrawQuad({ position.x, position.y, 0 }, size, color);
	}

	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color) {
		std::dynamic_pointer_cast<Wraith::OpenGLShader>(s_Data->FlatColorShader)->Bind();
		std::dynamic_pointer_cast<Wraith::OpenGLShader>(s_Data->FlatColorShader)->UploadUniformFloat4("u_Color", color);

		s_Data->QuadVertexArray->Bind();
		RenderCommand::DrawIndexed(s_Data->QuadVertexArray);
	}
}