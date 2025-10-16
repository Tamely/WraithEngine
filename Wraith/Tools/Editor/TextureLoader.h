#pragma once

#include "Rendering/Texture.h"
#include <unordered_map>
#include <string>

#include <ImGui-Premake/imgui.h>

namespace Wraith {

	class TextureLoader {
	public:
		static TextureLoader& Instance() {
			static TextureLoader instance;
			return instance;
		}

		ImTextureID LoadTexture(const std::string& filePath, bool flipVertically = false);
		void UnloadTexture(const std::string& filePath, bool flipVertically = false);
		void UnloadAllTextures();
		bool ReloadTexture(const std::string& filePath, bool flipVertically = false);
		bool IsTextureLoaded(const std::string& filePath, bool flipVertically = false) const;

		Ref<Texture2D> GetTexture(const std::string& filePath, bool flipVertically = false);

		std::pair<uint32_t, uint32_t> GetTextureDimensions(const std::string& filePath, bool flipVertically = false) const;

		uint32_t GetTextureWidth(ImTextureID loadedID) const;
		uint32_t GetTextureHeight(ImTextureID loadedID) const;

		ImTextureID GetWhiteTexture();
		size_t GetLoadedTextureCount() const { return m_LoadedTextures.size(); }

	private:
		TextureLoader() = default;
		~TextureLoader() = default;

		std::string GetTextureKey(const std::string& filePath, bool flipVertically) const;

		std::unordered_map<std::string, Ref<Texture2D>> m_LoadedTextures;
		std::unordered_map<ImTextureID, Ref<Texture2D>> m_LoadedTexturesByID;
		Ref<Texture2D> m_WhiteTexture = nullptr;
	};

	// Convenience functions
	inline ImTextureID LoadTexture(const std::string& filePath, bool flipVertically = false) {
		return TextureLoader::Instance().LoadTexture(filePath, flipVertically);
	}

	inline void UnloadTexture(const std::string& filePath, bool flipVertically = false) {
		TextureLoader::Instance().UnloadTexture(filePath, flipVertically);
	}
}
