#pragma once

#include "Core/Pin.h"
#include "Core/Node.h"
#include "Core/Connection.h"
#include "Core/NodeGraph.h"
#include "Core/Vertex2D.h"

#include <Wraith.h>

namespace Wraith {
	class NodeEditorRenderer {
	public:
		void Init();
		void Shutdown();

		void BeginScene(const glm::mat4& view, const glm::mat4& projection);
		void EndScene();
		
		void DrawNode(const Node& node, bool isHovered, bool isSelected);

		void DrawConnection(const Connection& connection, const NodeGraph& graph);
		void DrawConnectionPreview(glm::vec2 start, glm::vec2 end, const glm::vec4& color);

		void DrawPin(const Pin& pin, bool isHovered);

		void DrawGrid(float gridSize);
		void DrawSelectionBox(glm::vec2 start, glm::vec2 end);

	private:
		std::vector<glm::vec2> CalculateBezierCurve(glm::vec2 start, glm::vec2 end, int segments = 50);

		glm::vec4 GetPinColorForType(PinType type);

		void SetupQuadVertexArray();
		void SetupLineVertexArray();
		void SetupCircleVertexArray();

		void CreateShaders();

		void DrawQuad(glm::vec2 position, glm::vec2 size, const glm::vec4& color, float cornerRadius = 0.0f);
		void DrawCircle(glm::vec2 center, float radius, const glm::vec4& color);
		void DrawLine(glm::vec2 start, glm::vec2 end, const glm::vec4& color, float width);

		void FlushLines();

	private:
		Ref<Shader> m_LineShader;
		Ref<Shader> m_QuadShader;
		Ref<Shader> m_CircleShader;

		Ref<VertexArray> m_QuadVA;
		Ref<VertexArray> m_LineVA;
		Ref<VertexArray> m_CircleVA;

		Ref<VertexBuffer> m_DynamicLineVB;

		std::vector<Vertex2D> m_LineVertices;

		// TODO: Font rendering/text rendering system
		// Ref<Font> m_Font;

		glm::mat4 m_ViewMatrix;
		glm::mat4 m_ProjectionMatrix;

		uint32_t m_HoveredPinID;

		const glm::vec4 m_NodeSelectedColor = { 1.0f, 0.0f, 1.0f, 1.0f };
		const glm::vec4 m_NodeHoveredColor = { 0.0f, 1.0f, 1.0f, 1.0f };
		const glm::vec4 m_PinConnectedColor = { 1.0f, 0.0f, 1.0f, 1.0f };
		const glm::vec4 m_PinHoveredColor = { 0.0f, 1.0f, 1.0f, 1.0f };
		const glm::vec4 m_BorderColor = { 1.0f, 0.8f, 0.2f, 1.0f };
		const glm::vec4 m_SelectionBoxColor = { 0.2f, 0.6f, 1.0f, 0.2f };
		const glm::vec4 m_SelectionBoxBorderColor = { 0.2f, 0.6f, 1.0f, 0.8f };
		const glm::vec4 m_GridColor = { 0.2f, 0.2f, 0.2f, 0.3f };
	};
}
