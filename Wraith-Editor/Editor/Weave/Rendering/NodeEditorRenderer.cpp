#include "NodeEditorRenderer.h"

#include <Wraith.h>
#include <Core/Log.h>
#include <vector>
#define _USE_MATH_DEFINES
#include <math.h>

namespace Wraith {
	void NodeEditorRenderer::Init() {
		SetupQuadVertexArray();
		SetupLineVertexArray();
		SetupCircleVertexArray();

		W_INFO("NodeEditorRenderer initialized successfully");
	}

	void NodeEditorRenderer::Shutdown() {
		// Resources SHOULD be cleaned up automatically by smart pointers
		W_INFO("NodeEditorRenderer shut down");
	}

	void NodeEditorRenderer::BeginScene(const glm::mat4& view, const glm::mat4& projection) {
		m_ViewMatrix = view;
		m_ProjectionMatrix = projection;

		// Clear line vertices for this frame
		m_LineVertices.clear();
	}

	void NodeEditorRenderer::EndScene() {
		FlushLines();
	}

	void NodeEditorRenderer::DrawNode(const Node& node, bool isHovered, bool isSelected) {
		glm::vec4 color = node.Color;

		if (isSelected) {
			color = m_NodeSelectedColor;
		} 
		else if (isHovered) {
			color = m_NodeHoveredColor;
		}

		DrawQuad(node.Position, node.Size, color, 8.0f);

		if (isSelected) {
			DrawQuad(node.Position - glm::vec2(2.0f), node.Size + glm::vec2(4.0f), m_BorderColor, 8.0f);
			DrawQuad(node.Position, node.Size, color, 8.0f);
		}

		// Draw header section
		glm::vec4 headerColor = color * 0.8f;
		headerColor.w = color.w;
		DrawQuad(node.Position, { node.Size.x, 25.0f }, headerColor, 8.0f);

		// TODO: Draw node title text here when we add a font renderer

		// Draw pins
		for (const auto& pin : node.InputPins) {
			DrawPin(pin, pin.ID == m_HoveredPinID);
		}
		for (const auto& pin : node.OutputPins) {
			DrawPin(pin, pin.ID == m_HoveredPinID);
		}
	}

	void NodeEditorRenderer::DrawConnection(const Connection& connection, const NodeGraph& graph) {
		const Pin* fromPin = nullptr;
		const Pin* toPin = nullptr;

		// Search through all nodes to find the pins - maybe cache this
		for (const auto& [nodeId, node] : graph.GetNodes()) {
			for (const auto& pin : node.InputPins) {
				if (pin.ID == connection.ToPinID) toPin = &pin;
			}
			for (const auto& pin : node.OutputPins) {
				if (pin.ID == connection.ToPinID) fromPin = &pin;
			}
		}

		if (!fromPin || !toPin) return;

		auto bezierPoints = CalculateBezierCurve(fromPin->Position, toPin->Position);

		// Upload line verticies and draw
		// TODO: Create a dynamic vertex buffer for the vertex curve

		for (size_t i = 0; i < bezierPoints.size() - 1; ++i) {
			DrawLine(bezierPoints[i], bezierPoints[i + 1], connection.Color, 3.0f);
		}
	}

	void NodeEditorRenderer::DrawConnectionPreview(glm::vec2 start, glm::vec2 end, const glm::vec4& color) {
		auto bezierPoints = CalculateBezierCurve(start, end);

		for (size_t i = 0; i < bezierPoints.size() - 1; ++i) {
			DrawLine(bezierPoints[i], bezierPoints[i + 1], color, 2.0f);
		}
	}

	void NodeEditorRenderer::DrawPin(const Pin& pin, bool isHovered) {
		glm::vec4 pinColor = GetPinColorForType(pin.Type);

		if (!pin.ConnectedPins.empty()) {
			pinColor = m_PinConnectedColor;
		}

		if (isHovered) {
			pinColor = m_PinHoveredColor;
		}

		glm::vec4 outlineColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		DrawCircle(pin.WorldPosition, pin.Radius + 1.0f, outlineColor);
		DrawCircle(pin.WorldPosition, pin.Radius, pinColor);

		// TODO: Draw pin name text
	}

	void NodeEditorRenderer::DrawGrid(float gridSize) {
		glm::vec2 viewMin = glm::vec2(-m_ViewMatrix[3]) - glm::vec2(1000.0f); // Simplified bounds calculation
		glm::vec2 viewMax = glm::vec2(-m_ViewMatrix[3]) + glm::vec2(1000.0f); // Simplified bounds calculation

		// Draw vertical lines
		float startX = floor(viewMin.x / gridSize) * gridSize;
		float endX = ceil(viewMax.x / gridSize) * gridSize;

		for (float x = startX; x <= endX; x += gridSize) {
			DrawLine({ x, viewMin.y }, { x, viewMax.y }, m_GridColor, 1.0f);
		}

		// Draw horizontal lines
		float startY = floor(viewMin.y / gridSize) * gridSize;
		float endY = ceil(viewMax.y / gridSize) * gridSize;

		for (float y = startY; y <= endY; y += gridSize) {
			DrawLine({ viewMin.x, y }, { viewMax.x, y }, m_GridColor, 1.0f);
		}

		FlushLines();
	}

	void NodeEditorRenderer::DrawSelectionBox(glm::vec2 start, glm::vec2 end) {
		glm::vec2 size = end - start;

		DrawQuad(start, size, m_SelectionBoxColor);

		DrawLine(start, { end.x, start.y }, m_SelectionBoxBorderColor, 1.0f);
		DrawLine({ end.x, start.y }, end, m_SelectionBoxBorderColor, 1.0f);
		DrawLine(end, { start.x, end.y }, m_SelectionBoxBorderColor, 1.0f);
		DrawLine({ start.x, end.y }, start, m_SelectionBoxBorderColor, 1.0f);

		FlushLines();
	}

	std::vector<glm::vec2> NodeEditorRenderer::CalculateBezierCurve(glm::vec2 start, glm::vec2 end, int segments) {
		std::vector<glm::vec2> points;
		points.reserve(segments + 1);

		// Create control points for a nice curve
		float distance = glm::length(end - start);
		float controlOffset = glm::min(distance * 0.5f, 100.0f);

		glm::vec2 control1 = start + glm::vec2(controlOffset, 0.0f);
		glm::vec2 control2 = end - glm::vec2(controlOffset, 0.0f);

		// Generate bezier curve points
		for (int i = 0; i <= segments; ++i) {
			float t = static_cast<float>(i) / segments;
			float oneMinusT = 1.0f - t;

			glm::vec2 point =	oneMinusT * oneMinusT * oneMinusT * start +
								3.0f * oneMinusT * oneMinusT * t * control1 +
								3.0f * oneMinusT * t * t * control2 +
								t * t * t * end;

			points.push_back(point);
		}

		return points;
	}

	glm::vec4 NodeEditorRenderer::GetPinColorForType(PinType type) {
		switch (type) {
			case PinType::Float:   return { 0.4f, 0.8f, 0.4f, 1.0f }; // Green
			case PinType::Vec2:    return { 0.8f, 0.8f, 0.4f, 1.0f }; // Yellow
			case PinType::Vec3:    return { 0.8f, 0.6f, 0.4f, 1.0f }; // Orange
			case PinType::Vec4:    return { 0.8f, 0.4f, 0.4f, 1.0f }; // Red
			case PinType::Int:     return { 0.4f, 0.6f, 0.8f, 1.0f }; // Blue
			case PinType::Bool:    return { 0.8f, 0.4f, 0.8f, 1.0f }; // Magenta
			case PinType::String:  return { 0.6f, 0.4f, 0.8f, 1.0f }; // Purple
			case PinType::Exec:    return { 1.0f, 1.0f, 1.0f, 1.0f }; // White
			default:               return { 0.5f, 0.5f, 0.5f, 1.0f }; // Gray
		}
	}

	void NodeEditorRenderer::SetupQuadVertexArray() {
		float vertices[] = {
			// Position     TexCoord
			0.0f, 0.0f,		0.0f, 0.0f, // Bottom left
			1.0f, 0.0f,		1.0f, 0.0f, // Bottom right
			1.0f, 1.0f,		1.0f, 1.0f, // Top left
			0.0f, 1.0f,		0.0f, 1.0f, // Top right
		};

		uint32_t indices[] = {
			0, 1, 2,
			2, 3, 0
		};

		m_QuadVA = VertexArray::Create();

		auto vertexBuffer = VertexBuffer::Create(vertices, sizeof(vertices));
		vertexBuffer->SetLayout({
			{ ShaderDataType::Float2, "a_Position" },
			{ ShaderDataType::Float2, "a_TexCoord" },
		});

		auto indexBuffer = IndexBuffer::Create(indices, sizeof(indices) / sizeof(uint32_t));

		m_QuadVA->AddVertexBuffer(vertexBuffer);
		m_QuadVA->SetIndexBuffer(indexBuffer);
	}

	void NodeEditorRenderer::SetupLineVertexArray() {
		m_LineVA = VertexArray::Create();

		// Create empty vertex buffer that we'll update dynamically
		m_DynamicLineVB = VertexBuffer::Create(nullptr, 1000 * sizeof(Vertex2D)); // Preallocate space for 1000 vertices (the flush count)
		m_DynamicLineVB->SetLayout({
			{ ShaderDataType::Float2, "a_Position" },
			{ ShaderDataType::Float2, "a_TexCoord" },
			{ ShaderDataType::Float4, "a_Color" }
		});

		m_LineVA->AddVertexBuffer(m_DynamicLineVB);
	}

	void NodeEditorRenderer::SetupCircleVertexArray() {
		const int segments = 32;
		std::vector<float> vertices;
		std::vector<uint32_t> indices;

		vertices.insert(vertices.end(), { 0.0f, 0.0f, 0.5f, 0.5f }); // Pos, TexCoord

		for (int i = 0; i <= segments; ++i) {
			float angle = 2.0f * M_PI * i / segments;
			float x = cos(angle);
			float y = sin(angle);
			vertices.insert(vertices.end(), { x, y, (x + 1.0f) * 0.5f, (y + 1.0f) * 0.5f });
		}

		for (int i = 1; i <= segments; ++i) {
			indices.insert(indices.end(), { 0, static_cast<uint32_t>(i), static_cast<uint32_t>(i + 1) });
		}

		m_CircleVA = VertexArray::Create();

		auto vertexBuffer = VertexBuffer::Create(vertices.data(), vertices.size() * sizeof(float));
		vertexBuffer->SetLayout({
			{ ShaderDataType::Float2, "a_Position" },
			{ ShaderDataType::Float2, "a_TexCoord" }
		});

		auto indexBuffer = IndexBuffer::Create(indices.data(), indices.size());

		m_CircleVA->AddVertexBuffer(vertexBuffer);
		m_CircleVA->SetIndexBuffer(indexBuffer);
	}

	void NodeEditorRenderer::CreateShaders() {
		m_QuadShader = Shader::Create("Content/Shaders/WeaveRenderer_Quad.glsl");
		m_CircleShader = Shader::Create("Content/Shaders/WeaveRenderer_Circle.glsl");
		m_LineShader = Shader::Create("Content/Shaders/WeaveRenderer_Line.glsl");
	}

	void NodeEditorRenderer::DrawQuad(glm::vec2 position, glm::vec2 size, const glm::vec4& color, float cornerRadius) {
		glm::mat4 transform =	glm::translate(glm::mat4(1.0f), glm::vec3(position, 0.0f)) *
								glm::scale(glm::mat4(1.0f), glm::vec3(size, 1.0f));

		m_QuadShader->Bind();
		m_QuadShader->SetMat4("u_ViewProjection", m_ProjectionMatrix * m_ViewMatrix);
		m_QuadShader->SetMat4("u_Transform", transform);
		m_QuadShader->SetFloat4("u_Color", color);
		m_QuadShader->SetFloat("u_CornerRadius", cornerRadius);
		m_QuadShader->SetFloat2("u_Size", size);

		RenderCommand::DrawIndexed(m_QuadVA);
	}

	void NodeEditorRenderer::DrawCircle(glm::vec2 center, float radius, const glm::vec4& color) {
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(center, 0.0f)) * 
							  glm::scale(glm::mat4(1.0f), glm::vec3(radius, radius, 1.0f));

		m_CircleShader->Bind();
		m_CircleShader->SetMat4("u_ViewProjection", m_ProjectionMatrix * m_ViewMatrix);
		m_CircleShader->SetMat4("u_Transform", transform);
		m_CircleShader->SetFloat4("u_Color", color);

		RenderCommand::DrawIndexed(m_CircleVA);
	}

	void NodeEditorRenderer::DrawLine(glm::vec2 start, glm::vec2 end, const glm::vec4& color, float width) {
		// Add the vertices to the batch
		Vertex2D startVertex = { start, { 0.0f, 0.0f }, color };
		Vertex2D endVertex = { end, { 1.0f, 0.0f }, color };

		m_LineVertices.push_back(startVertex);
		m_LineVertices.push_back(endVertex);

		// If we have too many vertices in the batch, flush them
		if (m_LineVertices.size() >= 1000) {
			FlushLines();
		}
	}

	void NodeEditorRenderer::FlushLines() {
		if (m_LineVertices.empty()) return;

		// Update the dynamic vertex buffer
		m_DynamicLineVB->SetData(m_LineVertices.data(), m_LineVertices.size() * sizeof(Vertex2D));

		m_LineShader->Bind();
		m_LineShader->SetMat4("u_ViewProjection", m_ProjectionMatrix * m_ViewMatrix);

		RenderCommand::SetLineWidth(2.0f);
		RenderCommand::DrawLines(m_LineVA, m_LineVertices.size());
		RenderCommand::SetLineWidth(1.0f);

		// Clear for next batch
		m_LineVertices.clear();
	}
}
