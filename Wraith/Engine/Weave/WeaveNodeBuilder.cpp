#include "wpch.h"
#include "WeaveNodeBuilder.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>

namespace Wraith {
	WeaveNodeBuilder::WeaveNodeBuilder(ImTextureID texture, int textureWidth, int textureHeight)
		: m_HeaderTextureId(texture), m_HeaderTextureWidth(textureWidth), m_HeaderTextureHeight(textureHeight)
		, m_CurrentNodeId(0), m_CurrentStage(Stage::Invalid), m_HasHeader(false) {}

	void WeaveNodeBuilder::Begin(ax::NodeEditor::NodeId id) {
		m_HasHeader = false;
		m_HeaderMin = m_HeaderMax = ImVec2();

		ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_NodePadding, ImVec4(8, 4, 8, 8));

		ax::NodeEditor::BeginNode(id);

		ImGui::PushID(id.AsPointer());
		m_CurrentNodeId = id;

		SetStage(Stage::Begin);
	}

	void WeaveNodeBuilder::End() {
		SetStage(Stage::End);

		ax::NodeEditor::EndNode();

		if (ImGui::IsItemVisible()) {
			int alpha = static_cast<int>(255 * ImGui::GetStyle().Alpha);
			ImDrawList* drawList = ax::NodeEditor::GetNodeBackgroundDrawList(m_CurrentNodeId);
			const float halfBorderWidth = ax::NodeEditor::GetStyle().NodeBorderWidth * 0.5f;

			uint32_t headerColor = IM_COL32(0, 0, 0, alpha) | (m_HeaderColor & IM_COL32(255, 255, 255, 0));
			if ((m_HeaderMax.x > m_HeaderMin.x) && (m_HeaderMax.y > m_HeaderMin.y) && m_HeaderTextureId) {
				const ImVec2 uv = ImVec2(
					(m_HeaderMax.x - m_HeaderMin.x) / (float)(4.0f * m_HeaderTextureWidth),
					(m_HeaderMax.y - m_HeaderMin.y) / (float)(4.0f * m_HeaderTextureHeight)
				);

				drawList->AddImageRounded(m_HeaderTextureId,
										  m_HeaderMin - ImVec2(8 - halfBorderWidth, 4 - halfBorderWidth),
										  m_HeaderMax + ImVec2(8 - halfBorderWidth, 0),
										  ImVec2(0.0f, 0.0f), uv, headerColor, ax::NodeEditor::GetStyle().NodeRounding, ImDrawFlags_RoundCornersTop
				);

				if (m_ContentMin.y > m_HeaderMax.y) {
					drawList->AddLine(
						ImVec2(m_HeaderMin.x - (8 - halfBorderWidth), m_HeaderMax.y - 0.5f),
						ImVec2(m_HeaderMax.x - (8 - halfBorderWidth), m_HeaderMax.y - 0.5f),
						ImColor(255, 255, 255, 96 * alpha / (3 * 255)), 1.0f
					);
				}
			}
		}

		m_CurrentNodeId = 0;
		ImGui::PopID();

		ax::NodeEditor::PopStyleVar();

		SetStage(Stage::Invalid);
	}

	void WeaveNodeBuilder::Header(const ImVec4& color) {
		m_HeaderColor = ImColor(color);
		SetStage(Stage::Header);
	}

	void WeaveNodeBuilder::EndHeader() {
		SetStage(Stage::Content);
	}

	void WeaveNodeBuilder::Input(ax::NodeEditor::PinId id) {
		if (m_CurrentStage == Stage::Begin) SetStage(Stage::Content);

		const bool bShouldApplyPadding = m_CurrentStage == Stage::Input;

		SetStage(Stage::Input);

		if (bShouldApplyPadding) ImGui::Spring(0);

		Pin(id, ax::NodeEditor::PinKind::Input);

		ImGui::BeginHorizontal(id.AsPointer());
	}

	void WeaveNodeBuilder::EndInput() {
		ImGui::EndHorizontal();
		EndPin();
	}

	void WeaveNodeBuilder::Middle() {
		if (m_CurrentStage == Stage::Begin) SetStage(Stage::Content);

		SetStage(Stage::Middle);
	}

	void WeaveNodeBuilder::Output(ax::NodeEditor::PinId id) {
		if (m_CurrentStage == Stage::Begin) SetStage(Stage::Content);

		const bool bShouldApplyPadding = m_CurrentStage == Stage::Output;

		SetStage(Stage::Output);

		if (bShouldApplyPadding) ImGui::Spring(0);

		Pin(id, ax::NodeEditor::PinKind::Output);

		ImGui::BeginHorizontal(id.AsPointer());
	}

	void WeaveNodeBuilder::EndOutput() {
		ImGui::EndHorizontal();
		EndPin();
	}

	bool WeaveNodeBuilder::SetStage(Stage stage) {
		if (stage == m_CurrentStage) return false;

		Stage oldStage = m_CurrentStage;
		m_CurrentStage = stage;

		ImVec2 cursor;
		switch (oldStage) {
			case Stage::Begin: break;
			case Stage::Header:
				ImGui::EndHorizontal();
				m_HeaderMin = ImGui::GetItemRectMin();
				m_HeaderMax = ImGui::GetItemRectMax();

				// Spacing between header and content
				ImGui::Spring(0, ImGui::GetStyle().ItemSpacing.y * 2.0f);
				break;
			case Stage::Content: break;
			case Stage::Input:
				ax::NodeEditor::PopStyleVar(2);

				ImGui::Spring(1, 0);
				ImGui::EndVertical();
				break;
			case Stage::Middle:
				ImGui::EndVertical();
				break;
			case Stage::Output:
				ax::NodeEditor::PopStyleVar(2);

				ImGui::Spring(1, 0);
				ImGui::EndVertical();
				break;
			case Stage::End: break;
			case Stage::Invalid: break;
		}

		switch (stage) {
			case Stage::Begin:
				ImGui::BeginVertical("node");
				break;
			case Stage::Header:
				m_HasHeader = true;
				ImGui::BeginHorizontal("header");
				break;
			case Stage::Content:
				if (oldStage == Stage::Begin) ImGui::Spring(0);
				
				ImGui::BeginHorizontal("content");
				ImGui::Spring(0, 0);
				break;
			case Stage::Input:
				ImGui::BeginVertical("inputs", ImVec2(0, 0), 0.0f);

				ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_PivotAlignment, ImVec2(0, 0.5f));
				ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_PivotSize, ImVec2(0, 0));

				if (!m_HasHeader) ImGui::Spring(1, 0);
				break;
			case Stage::Middle:
				ImGui::Spring(1);
				ImGui::BeginVertical("middle", ImVec2(0, 0), 1.0f);
				break;
			case Stage::Output:
				if (oldStage == Stage::Middle || oldStage == Stage::Input) ImGui::Spring(1);
				else ImGui::Spring(1, 0);
					
				ImGui::BeginVertical("outputs", ImVec2(0, 0), 1.0f);

				ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_PivotAlignment, ImVec2(1.0f, 0.5f));
				ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_PivotSize, ImVec2(0, 0));

				if (!m_HasHeader) ImGui::Spring(1, 0);
				break;
			case Stage::End:
				if (oldStage == Stage::Input) ImGui::Spring(1, 0);
				if (oldStage != Stage::Begin) ImGui::EndHorizontal();

				m_ContentMin = ImGui::GetItemRectMin();
				m_ContentMax = ImGui::GetItemRectMax();

				ImGui::EndVertical();
				m_NodeMin = ImGui::GetItemRectMin();
				m_NodeMax = ImGui::GetItemRectMax();
				break;
			case Stage::Invalid:
				break;
		}

		return true;
	}

	void WeaveNodeBuilder::Pin(ax::NodeEditor::PinId id, ax::NodeEditor::PinKind kind) {
		ax::NodeEditor::BeginPin(id, kind);
	}

	void WeaveNodeBuilder::EndPin() {
		ax::NodeEditor::EndPin();
	}
}
