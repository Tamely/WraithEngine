#include <Wraith.h>
#include <Core/EntryPoint.h>

#include "EditorLayer.h"
#include "WeaveLayer.h"

namespace Wraith {
	class WraithEditor : public Application {
	public:
		WraithEditor(ApplicationCommandLineArgs args)
			: Application("Wraith Editor", args) {
			PushLayer(new EditorLayer());
			PushLayer(new WeaveLayer());
		}

		~WraithEditor() {

		}
	};

	Application* CreateApplication(ApplicationCommandLineArgs args) {
		return new WraithEditor(args);
	}
}
