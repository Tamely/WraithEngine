#pragma once
#include "ComponentMacros.h"
#include "CoreObject/ScriptableEntity.h"


namespace Wraith {
	struct NativeScriptComponent {
		GENERATED_COMPONENT_BODY(NativeScriptComponent);

		ScriptableEntity* Instance = nullptr;

		ScriptableEntity* (*InstantiateScript)() = nullptr;
		void (*DestroyScript)(NativeScriptComponent*) = nullptr;

		template<typename T>
		void Bind() {
			InstantiateScript = []() { return static_cast<ScriptableEntity*>(new T()); };
			DestroyScript = [](NativeScriptComponent* nsc) { delete nsc->Instance; nsc->Instance = nullptr; };
		}
	};
}
