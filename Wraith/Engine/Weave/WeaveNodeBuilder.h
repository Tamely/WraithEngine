#pragma once

#include <ImGui-NodeEditor/imgui_node_editor.h>

namespace Wraith {
	struct WeaveNodeBuilder {
		WeaveNodeBuilder(ImTextureID texture = nullptr, int textureWidth = 0, int textureHeight = 0);

		void Begin(ax::NodeEditor::NodeId id);
		void End();

		void Header(const ImVec4& color = ImVec4(1, 1, 1, 1));
		void EndHeader();

		void Input(ax::NodeEditor::PinId id);
		void EndInput();

		void Middle();

		void Output(ax::NodeEditor::PinId id);
		void EndOutput();

	private:
		enum class Stage {
			Invalid,
			Begin,
			Header,
			Content,
			Input,
			Output,
			Middle,
			End
		};

		bool SetStage(Stage stage);

		void Pin(ax::NodeEditor::PinId id, ax::NodeEditor::PinKind kind);
		void EndPin();

	private:
		ImTextureID m_HeaderTextureId;
		int m_HeaderTextureWidth;
		int m_HeaderTextureHeight;
		ImU32 m_HeaderColor;

		ax::NodeEditor::NodeId m_CurrentNodeId;
		Stage m_CurrentStage;

		ImVec2 m_NodeMin;
		ImVec2 m_NodeMax;

		ImVec2 m_HeaderMin;
		ImVec2 m_HeaderMax;

		ImVec2 m_ContentMin;
		ImVec2 m_ContentMax;

		bool m_HasHeader;
	};
}
