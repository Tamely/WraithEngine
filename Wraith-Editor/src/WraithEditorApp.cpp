#include <Wraith.h>
#include <Wraith/Core/EntryPoint.h>

#include "EditorLayer.h"
#include "Game/GameLayer.h"

namespace Wraith {
	class WraithEditor : public Application {
	public:
		WraithEditor()
			: Application("Wraith Editor") {
			PushLayer(new GameLayer());
		}

		~WraithEditor() {

		}
	};

	Application* CreateApplication() {
		return new WraithEditor();
	}
}