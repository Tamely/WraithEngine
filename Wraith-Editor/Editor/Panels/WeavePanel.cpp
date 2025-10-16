#include "wpch.h"
#include "WeavePanel.h"

#include <Weave/WeaveWidgets.h>
#include <Weave/WeaveNodeBuilder.h>
#include <Weave/Registry/NodeRegistry.h>

#include <Editor/TextureLoader.h>

#include <imgui.h>
#include <imgui_internal.h>

#include "Nodes/AddNode.h"

namespace Wraith {
	WeavePanel::WeavePanel() {
		ax::NodeEditor::Config config;
		config.SettingsFile = "Weave.json";
		config.UserPointer = this;

		config.LoadNodeSettings = [](ax::NodeEditor::NodeId nodeId, char* data, void* userPointer) -> size_t {
			auto self = static_cast<WeavePanel*>(userPointer);
			auto node = self->FindNode(nodeId);
			if (!node) return 0;
			if (data != nullptr)
				memcpy(data, node->State.data(), node->State.size());
			return node->State.size();
		};

		config.SaveNodeSettings = [](ax::NodeEditor::NodeId nodeId, const char* data, size_t size, ax::NodeEditor::SaveReasonFlags reason, void* userPointer) -> bool {
			auto self = static_cast<WeavePanel*>(userPointer);
			auto node = self->FindNode(nodeId);
			if (!node) return false;
			node->State.assign(data, size);
			self->TouchNode(nodeId);
			return true;
		};

		m_EditorContext = ax::NodeEditor::CreateEditor(&config);

		m_HeaderBackground = TextureLoader::Instance().LoadTexture("Content/Textures/Icons/Weave/Background.png");

		ax::NodeEditor::SetCurrentEditor(m_EditorContext);
		Node* node;
		
		node = SpawnNodeFromRegistry("Add");
		if (node) ax::NodeEditor::SetNodePosition(node->ID, ImVec2(-252, 220));

		ax::NodeEditor::NavigateToContent();
		ax::NodeEditor::SetCurrentEditor(nullptr);
	}

	WeavePanel::~WeavePanel() {
		if (m_EditorContext) {
			ax::NodeEditor::DestroyEditor(m_EditorContext);
			m_EditorContext = nullptr;
		}
	}

	void WeavePanel::OnUpdate(float deltaTime) {
		for (auto& entry : m_NodeTouchTime) {
			if (entry.second > 0.0f) {
				entry.second -= deltaTime;
			}
		}
	}

	Node* WeavePanel::SpawnNodeFromRegistry(const std::string& definitionName) {
		auto& registry = NodeRegistry::Instance();

		Node node = registry.CreateNode(definitionName, m_NextId);

		if (node.Name == "Unknown") {
			return nullptr;
		}

		m_Nodes.push_back(node);

		BuildNode(&m_Nodes.back());
		return &m_Nodes.back();
	}

	void WeavePanel::RenderContextMenu(ImVec2 openPopupPosition, Pin* newNodeLinkPin, bool& createNewNode) {
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
		if (ImGui::BeginPopup("Create New Node")) {
			auto& registry = NodeRegistry::Instance();
			const auto& categories = registry.GetCategories();

			Node* spawnedNode = nullptr;
			std::string nodeToSpawn;

			for (const auto& [category, nodeNames] : categories) {
				if (ImGui::BeginMenu(category.c_str())) {
					for (const auto& nodeName : nodeNames) {
						if (ImGui::MenuItem(nodeName.c_str())) {
							nodeToSpawn = nodeName;
						}
					}
					ImGui::EndMenu();
				}
			}

			if (!nodeToSpawn.empty()) {
				spawnedNode = SpawnNodeFromRegistry(nodeToSpawn);

				if (spawnedNode) {
					createNewNode = false;
					ax::NodeEditor::SetNodePosition(spawnedNode->ID, openPopupPosition);

					if (auto startPin = newNodeLinkPin) {
						auto& pins = startPin->Kind == PinKind::Input ? spawnedNode->Outputs : spawnedNode->Inputs;
						for (auto& pin : pins) {
							if (CanCreateLink(startPin, &pin)) {
								auto endPin = &pin;
								if (startPin->Kind == PinKind::Input)
									std::swap(startPin, endPin);
								m_Links.emplace_back(Link(GetNextId(), startPin->ID, endPin->ID));
								m_Links.back().Color = GetIconColor(startPin->Type);
								break;
							}
						}
					}
				}
			}
			ImGui::EndPopup();
		}
		else {
			createNewNode = false;
		}
		ImGui::PopStyleVar();
	}

	void WeavePanel::OnImGuiRender() {
		ImGui::Begin("Weave Editor");
		ax::NodeEditor::SetCurrentEditor(m_EditorContext);

		static ax::NodeEditor::NodeId contextNodeId = 0;
		static ax::NodeEditor::LinkId contextLinkId = 0;
		static ax::NodeEditor::PinId contextPinId = 0;
		static bool createNewNode = false;
		static Pin* newNodeLinkPin = nullptr;
		static Pin* newLinkPin = nullptr;

		ax::NodeEditor::Begin("Weave Editor");
		{
			WeaveNodeBuilder builder(m_HeaderBackground,
				TextureLoader::Instance().GetTextureWidth(m_HeaderBackground),
				TextureLoader::Instance().GetTextureHeight(m_HeaderBackground));

			// Node rendering
			for (auto& node : m_Nodes) {
				if (node.Type != NodeType::Blueprint && node.Type != NodeType::Simple)
					continue;

				const auto isSimple = node.Type == NodeType::Simple;

				builder.Begin(node.ID);
				if (!isSimple) {
					builder.Header(node.Color);
					ImGui::Spring(0);
					ImGui::TextUnformatted(node.Name.c_str());
					ImGui::Spring(1);
					ImGui::Dummy(ImVec2(0, 28));
					ImGui::Spring(0);
					builder.EndHeader();
				}

				for (auto& input : node.Inputs) {
					auto alpha = ImGui::GetStyle().Alpha;
					if (newLinkPin && !CanCreateLink(newLinkPin, &input) && &input != newLinkPin)
						alpha = alpha * (48.0f / 255.0f);

					builder.Input(input.ID);
					ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
					DrawPinIcon(input, IsPinLinked(input.ID), (int)(alpha * 255));
					ImGui::Spring(0);
					if (!input.Name.empty()) {
						ImGui::TextUnformatted(input.Name.c_str());
						ImGui::Spring(0);
					}
					ImGui::PopStyleVar();
					builder.EndInput();
				}

				if (isSimple) {
					builder.Middle();
					ImGui::Spring(1, 0);
					ImGui::TextUnformatted(node.Name.c_str());
					ImGui::Spring(1, 0);
				}

				for (auto& output : node.Outputs) {
					auto alpha = ImGui::GetStyle().Alpha;
					if (newLinkPin && !CanCreateLink(newLinkPin, &output) && &output != newLinkPin)
						alpha = alpha * (48.0f / 255.0f);

					ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
					builder.Output(output.ID);
					if (!output.Name.empty()) {
						ImGui::Spring(0);
						ImGui::TextUnformatted(output.Name.c_str());
					}
					ImGui::Spring(0);
					DrawPinIcon(output, IsPinLinked(output.ID), (int)(alpha * 255));
					ImGui::PopStyleVar();
					builder.EndOutput();
				}

				builder.End();
			}

			// Link rendering
			for (auto& link : m_Links)
				ax::NodeEditor::Link(link.ID, link.StartPinID, link.EndPinID, link.Color, 2.0f);

			// Handle creation and deletion
			if (!createNewNode) {
				if (ax::NodeEditor::BeginCreate(ImColor(255, 255, 255), 2.0f)) {
					ax::NodeEditor::PinId startPinId = 0, endPinId = 0;
					if (ax::NodeEditor::QueryNewLink(&startPinId, &endPinId)) {
						auto startPin = FindPin(startPinId);
						auto endPin = FindPin(endPinId);
						newLinkPin = startPin ? startPin : endPin;

						if (startPin && endPin && CanCreateLink(startPin, endPin)) {
							if (ax::NodeEditor::AcceptNewItem(ImColor(128, 255, 128), 4.0f)) {
								if (startPin->Kind == PinKind::Input) std::swap(startPin, endPin);
								m_Links.emplace_back(Link(GetNextId(), startPin->ID, endPin->ID));
								m_Links.back().Color = GetIconColor(startPin->Type);
							}
						}
						else {
							ax::NodeEditor::RejectNewItem(ImColor(255, 0, 0), 2.0f);
						}
					}

					ax::NodeEditor::PinId pinId = 0;
					if (ax::NodeEditor::QueryNewNode(&pinId)) {
						newLinkPin = FindPin(pinId);
						if (ax::NodeEditor::AcceptNewItem()) {
							createNewNode = true;
							newNodeLinkPin = FindPin(pinId);
							newLinkPin = nullptr;
							ax::NodeEditor::Suspend();
							ImGui::OpenPopup("Create New Node");
							ax::NodeEditor::Resume();
						}
					}
				}
				else {
					newLinkPin = nullptr;
				}
				ax::NodeEditor::EndCreate();

				if (ax::NodeEditor::BeginDelete()) {
					ax::NodeEditor::NodeId nodeId = 0;
					while (ax::NodeEditor::QueryDeletedNode(&nodeId)) {
						if (ax::NodeEditor::AcceptDeletedItem()) {
							auto id = std::find_if(m_Nodes.begin(), m_Nodes.end(), [nodeId](auto& node) { return node.ID == nodeId; });
							if (id != m_Nodes.end()) m_Nodes.erase(id);
						}
					}

					ax::NodeEditor::LinkId linkId = 0;
					while (ax::NodeEditor::QueryDeletedLink(&linkId)) {
						if (ax::NodeEditor::AcceptDeletedItem()) {
							auto id = std::find_if(m_Links.begin(), m_Links.end(), [linkId](auto& link) { return link.ID == linkId; });
							if (id != m_Links.end()) m_Links.erase(id);
						}
					}
				}
				ax::NodeEditor::EndDelete();
			}

			// Context menu
			auto openPopupPosition = ImGui::GetMousePos();
			ax::NodeEditor::Suspend();

			if (ax::NodeEditor::ShowBackgroundContextMenu()) {
				ImGui::OpenPopup("Create New Node");
				newNodeLinkPin = nullptr;
			}

			ax::NodeEditor::Resume();

			ax::NodeEditor::Suspend();
			RenderContextMenu(openPopupPosition, newNodeLinkPin, createNewNode);
			ax::NodeEditor::Resume();
		}
		ax::NodeEditor::End();
		ax::NodeEditor::SetCurrentEditor(nullptr);
		ImGui::End();
	}

	void WeavePanel::BuildNode(Node* node) {
		for (auto& input : node->Inputs) {
			input.Node = node;
			input.Kind = PinKind::Input;
		}
		for (auto& output : node->Outputs) {
			output.Node = node;
			output.Kind = PinKind::Output;
		}
	}

	bool WeavePanel::CanCreateLink(Pin* a, Pin* b) {
		if (!a || !b || a == b || a->Kind == b->Kind || a->Type != b->Type || a->Node == b->Node)
			return false;
		return true;
	}

	Node* WeavePanel::FindNode(ax::NodeEditor::NodeId id) {
		for (auto& node : m_Nodes)
			if (node.ID == id)
				return &node;
		return nullptr;
	}

	Link* WeavePanel::FindLink(ax::NodeEditor::LinkId id) {
		for (auto& link : m_Links)
			if (link.ID == id)
				return &link;
		return nullptr;
	}

	Pin* WeavePanel::FindPin(ax::NodeEditor::PinId id) {
		if (!id) return nullptr;
		for (auto& node : m_Nodes) {
			for (auto& pin : node.Inputs)
				if (pin.ID == id) return &pin;
			for (auto& pin : node.Outputs)
				if (pin.ID == id) return &pin;
		}
		return nullptr;
	}

	bool WeavePanel::IsPinLinked(ax::NodeEditor::PinId id) {
		if (!id) return false;
		for (auto& link : m_Links)
			if (link.StartPinID == id || link.EndPinID == id)
				return true;
		return false;
	}

	void WeavePanel::TouchNode(ax::NodeEditor::NodeId id) {
		m_NodeTouchTime[id] = m_TouchTime;
	}

	ImColor WeavePanel::GetIconColor(PinType type) {
		switch (type) {
			default:
			case PinType::Flow:     return ImColor(255, 255, 255);
			case PinType::Bool:     return ImColor(220, 48, 48);
			case PinType::Int:      return ImColor(68, 201, 156);
			case PinType::Float:    return ImColor(147, 226, 74);
			case PinType::String:   return ImColor(124, 21, 153);
			case PinType::Object:   return ImColor(51, 150, 215);
			case PinType::Function: return ImColor(218, 0, 183);
			case PinType::Delegate: return ImColor(255, 48, 48);
		}
	}

	void WeavePanel::DrawPinIcon(const Pin& pin, bool connected, int alpha) {
		Drawing::IconType iconType;
		ImColor color = GetIconColor(pin.Type);
		color.Value.w = alpha / 255.0f;

		switch (pin.Type) {
			case PinType::Flow:     iconType = Drawing::IconType::Flow;   break;
			case PinType::Bool:     iconType = Drawing::IconType::Circle; break;
			case PinType::Int:      iconType = Drawing::IconType::Circle; break;
			case PinType::Float:    iconType = Drawing::IconType::Circle; break;
			case PinType::String:   iconType = Drawing::IconType::Circle; break;
			case PinType::Object:   iconType = Drawing::IconType::Circle; break;
			case PinType::Function: iconType = Drawing::IconType::Circle; break;
			case PinType::Delegate: iconType = Drawing::IconType::Square; break;
			default: return;
		}

		Icon(ImVec2(static_cast<float>(m_PinIconSize), static_cast<float>(m_PinIconSize)),
			iconType, connected, color, ImColor(32, 32, 32, alpha));
	}

	int WeavePanel::GetNextId() {
		return m_NextId++;
	}

	ax::NodeEditor::LinkId WeavePanel::GetNextLinkId() {
		return ax::NodeEditor::LinkId(GetNextId());
	}

	float WeavePanel::GetTouchProgress(ax::NodeEditor::NodeId id) {
		auto it = m_NodeTouchTime.find(id);
		if (it != m_NodeTouchTime.end() && it->second > 0.0f)
			return (m_TouchTime - it->second) / m_TouchTime;
		return 0.0f;
	}

	void WeavePanel::UpdateTouch() {
		const auto deltaTime = ImGui::GetIO().DeltaTime;
		for (auto& entry : m_NodeTouchTime) {
			if (entry.second > 0.0f)
				entry.second -= deltaTime;
		}
	}

	// Node spawning implementations
	Node* WeavePanel::SpawnInputActionNode() {
		m_Nodes.emplace_back(GetNextId(), "OnBegin", ImColor(255, 128, 128));
		m_Nodes.back().Outputs.emplace_back(GetNextId(), "Start", PinType::Flow);
		BuildNode(&m_Nodes.back());
		return &m_Nodes.back();
	}

	Node* WeavePanel::SpawnBranchNode() {
		m_Nodes.emplace_back(GetNextId(), "Branch");
		m_Nodes.back().Inputs.emplace_back(GetNextId(), "", PinType::Flow);
		m_Nodes.back().Inputs.emplace_back(GetNextId(), "Condition", PinType::Bool);
		m_Nodes.back().Outputs.emplace_back(GetNextId(), "True", PinType::Flow);
		m_Nodes.back().Outputs.emplace_back(GetNextId(), "False", PinType::Flow);
		BuildNode(&m_Nodes.back());
		return &m_Nodes.back();
	}

	Node* WeavePanel::SpawnDoNNode() {
		m_Nodes.emplace_back(GetNextId(), "Do N");
		m_Nodes.back().Inputs.emplace_back(GetNextId(), "Enter", PinType::Flow);
		m_Nodes.back().Inputs.emplace_back(GetNextId(), "N", PinType::Int);
		m_Nodes.back().Inputs.emplace_back(GetNextId(), "Reset", PinType::Flow);
		m_Nodes.back().Outputs.emplace_back(GetNextId(), "Exit", PinType::Flow);
		m_Nodes.back().Outputs.emplace_back(GetNextId(), "Counter", PinType::Int);
		BuildNode(&m_Nodes.back());
		return &m_Nodes.back();
	}

	Node* WeavePanel::SpawnOutputActionNode() {
		m_Nodes.emplace_back(GetNextId(), "OutputAction");
		m_Nodes.back().Inputs.emplace_back(GetNextId(), "Sample", PinType::Float);
		m_Nodes.back().Outputs.emplace_back(GetNextId(), "Condition", PinType::Bool);
		m_Nodes.back().Inputs.emplace_back(GetNextId(), "Event", PinType::Delegate);
		BuildNode(&m_Nodes.back());
		return &m_Nodes.back();
	}

	Node* WeavePanel::SpawnPrintStringNode() {
		m_Nodes.emplace_back(GetNextId(), "Print String");
		m_Nodes.back().Inputs.emplace_back(GetNextId(), "", PinType::Flow);
		m_Nodes.back().Inputs.emplace_back(GetNextId(), "In String", PinType::String);
		m_Nodes.back().Outputs.emplace_back(GetNextId(), "", PinType::Flow);
		BuildNode(&m_Nodes.back());
		return &m_Nodes.back();
	}

	Node* WeavePanel::SpawnMessageNode() {
		m_Nodes.emplace_back(GetNextId(), "", ImColor(128, 195, 248));
		m_Nodes.back().Type = NodeType::Simple;
		m_Nodes.back().Outputs.emplace_back(GetNextId(), "Message", PinType::String);
		BuildNode(&m_Nodes.back());
		return &m_Nodes.back();
	}

	Node* WeavePanel::SpawnSetTimerNode() {
		m_Nodes.emplace_back(GetNextId(), "Set Timer", ImColor(128, 195, 248));
		m_Nodes.back().Inputs.emplace_back(GetNextId(), "", PinType::Flow);
		m_Nodes.back().Inputs.emplace_back(GetNextId(), "Object", PinType::Object);
		m_Nodes.back().Inputs.emplace_back(GetNextId(), "Function Name", PinType::Function);
		m_Nodes.back().Inputs.emplace_back(GetNextId(), "Time", PinType::Float);
		m_Nodes.back().Inputs.emplace_back(GetNextId(), "Looping", PinType::Bool);
		m_Nodes.back().Outputs.emplace_back(GetNextId(), "", PinType::Flow);
		BuildNode(&m_Nodes.back());
		return &m_Nodes.back();
	}

	Node* WeavePanel::SpawnLessNode() {
		m_Nodes.emplace_back(GetNextId(), "<", ImColor(128, 195, 248));
		m_Nodes.back().Type = NodeType::Simple;
		m_Nodes.back().Inputs.emplace_back(GetNextId(), "", PinType::Float);
		m_Nodes.back().Inputs.emplace_back(GetNextId(), "", PinType::Float);
		m_Nodes.back().Outputs.emplace_back(GetNextId(), "", PinType::Float);
		BuildNode(&m_Nodes.back());
		return &m_Nodes.back();
	}

	Node* WeavePanel::SpawnWeirdNode() {
		m_Nodes.emplace_back(GetNextId(), "o.O", ImColor(128, 195, 248));
		m_Nodes.back().Type = NodeType::Simple;
		m_Nodes.back().Inputs.emplace_back(GetNextId(), "", PinType::Float);
		m_Nodes.back().Outputs.emplace_back(GetNextId(), "", PinType::Float);
		m_Nodes.back().Outputs.emplace_back(GetNextId(), "", PinType::Float);
		BuildNode(&m_Nodes.back());
		return &m_Nodes.back();
	}

	Node* WeavePanel::SpawnTraceByChannelNode() {
		m_Nodes.emplace_back(GetNextId(), "Single Line Trace by Channel", ImColor(255, 128, 64));
		m_Nodes.back().Inputs.emplace_back(GetNextId(), "", PinType::Flow);
		m_Nodes.back().Inputs.emplace_back(GetNextId(), "Start", PinType::Flow);
		m_Nodes.back().Inputs.emplace_back(GetNextId(), "End", PinType::Int);
		m_Nodes.back().Inputs.emplace_back(GetNextId(), "Trace Channel", PinType::Float);
		m_Nodes.back().Inputs.emplace_back(GetNextId(), "Trace Complex", PinType::Bool);
		m_Nodes.back().Inputs.emplace_back(GetNextId(), "Actors to Ignore", PinType::Int);
		m_Nodes.back().Inputs.emplace_back(GetNextId(), "Draw Debug Type", PinType::Bool);
		m_Nodes.back().Inputs.emplace_back(GetNextId(), "Ignore Self", PinType::Bool);
		m_Nodes.back().Outputs.emplace_back(GetNextId(), "", PinType::Flow);
		m_Nodes.back().Outputs.emplace_back(GetNextId(), "Out Hit", PinType::Float);
		m_Nodes.back().Outputs.emplace_back(GetNextId(), "Return Value", PinType::Bool);
		BuildNode(&m_Nodes.back());
		return &m_Nodes.back();
	}

	Node* WeavePanel::SpawnTreeSequenceNode() {
		m_Nodes.emplace_back(GetNextId(), "Sequence");
		m_Nodes.back().Type = NodeType::Tree;
		m_Nodes.back().Inputs.emplace_back(GetNextId(), "", PinType::Flow);
		m_Nodes.back().Outputs.emplace_back(GetNextId(), "", PinType::Flow);
		BuildNode(&m_Nodes.back());
		return &m_Nodes.back();
	}

	Node* WeavePanel::SpawnTreeTaskNode() {
		m_Nodes.emplace_back(GetNextId(), "Move To");
		m_Nodes.back().Type = NodeType::Tree;
		m_Nodes.back().Inputs.emplace_back(GetNextId(), "", PinType::Flow);
		BuildNode(&m_Nodes.back());
		return &m_Nodes.back();
	}

	Node* WeavePanel::SpawnTreeTask2Node() {
		m_Nodes.emplace_back(GetNextId(), "Random Wait");
		m_Nodes.back().Type = NodeType::Tree;
		m_Nodes.back().Inputs.emplace_back(GetNextId(), "", PinType::Flow);
		BuildNode(&m_Nodes.back());
		return &m_Nodes.back();
	}

	Node* WeavePanel::SpawnComment() {
		m_Nodes.emplace_back(GetNextId(), "Test Comment");
		m_Nodes.back().Type = NodeType::Comment;
		m_Nodes.back().Size = ImVec2(300, 200);
		return &m_Nodes.back();
	}

	Node* WeavePanel::SpawnHoudiniTransformNode() {
		m_Nodes.emplace_back(GetNextId(), "Transform");
		m_Nodes.back().Type = NodeType::Houdini;
		m_Nodes.back().Inputs.emplace_back(GetNextId(), "", PinType::Flow);
		m_Nodes.back().Outputs.emplace_back(GetNextId(), "", PinType::Flow);
		BuildNode(&m_Nodes.back());
		return &m_Nodes.back();
	}

	Node* WeavePanel::SpawnHoudiniGroupNode() {
		m_Nodes.emplace_back(GetNextId(), "Group");
		m_Nodes.back().Type = NodeType::Houdini;
		m_Nodes.back().Inputs.emplace_back(GetNextId(), "", PinType::Flow);
		m_Nodes.back().Inputs.emplace_back(GetNextId(), "", PinType::Flow);
		m_Nodes.back().Outputs.emplace_back(GetNextId(), "", PinType::Flow);
		BuildNode(&m_Nodes.back());
		return &m_Nodes.back();
	}
}
