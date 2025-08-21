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

		// Load a texture and return ImGui texture ID
		ImTextureID LoadTexture(const std::string& filePath, bool flipVertically = false);

		// Get the Texture2D object for a loaded texture
		Ref<Texture2D> GetTexture(const std::string& filePath, bool flipVertically = false);

		// Check if texture is loaded
		bool IsTextureLoaded(const std::string& filePath, bool flipVertically = false) const;

		// Unload a specific texture
		void UnloadTexture(const std::string& filePath, bool flipVertically = false);

		// Unload all textures
		void UnloadAllTextures();

		// Reload a texture (useful for hot-reloading during development)
		bool ReloadTexture(const std::string& filePath, bool flipVertically = false);

		// Get texture dimensions
		std::pair<uint32_t, uint32_t> GetTextureDimensions(const std::string& filePath, bool flipVertically = false) const;

		// Create a white texture for missing files
		ImTextureID GetWhiteTexture();

		// Get statistics
		size_t GetLoadedTextureCount() const { return m_LoadedTextures.size(); }

	private:
		TextureLoader() = default;
		~TextureLoader() = default;

		// Generate unique key for texture with flip state
		std::string GetTextureKey(const std::string& filePath, bool flipVertically) const;

		std::unordered_map<std::string, Ref<Texture2D>> m_LoadedTextures;
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
