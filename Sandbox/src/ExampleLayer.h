#pragma once
#include "Wraith.h"

class ExampleLayer : public Wraith::Layer
{
public:
	ExampleLayer();
	virtual ~ExampleLayer() = default;

	virtual void OnAttach() override;
	virtual void OnDetach() override;

	void OnUpdate(Wraith::Timestep ts) override;
	virtual void OnImGuiRender() override;
	void OnEvent(Wraith::Event& e) override;

private:
	Wraith::ShaderLibrary m_ShaderLibrary;
	Wraith::Ref<Wraith::Shader> m_Shader;
	Wraith::Ref<Wraith::VertexArray> m_VertexArray;

	Wraith::Ref<Wraith::Shader> m_FlatColorShader;
	Wraith::Ref<Wraith::VertexArray> m_SquareVA;

	Wraith::Ref<Wraith::Texture2D> m_Texture, m_CatTexture;

	Wraith::OrthographicCameraController m_CameraController;
	glm::vec3 m_SquareColor = { 0.2f, 0.3f, 0.4f };
};