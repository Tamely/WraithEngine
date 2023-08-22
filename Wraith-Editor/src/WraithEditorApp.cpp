#include <Wraith.h>
#include <Wraith/Core/EntryPoint.h>

#include "EditorLayer.h"

namespace Wraith {
	class WraithEditor : public Application {
	public:
		WraithEditor()
			: Application("Wraith Editor") {
			PushLayer(new EditorLayer());
		}

		~WraithEditor() {

		}
	};

	Application* CreateApplication() {
		return new WraithEditor();
	}
}