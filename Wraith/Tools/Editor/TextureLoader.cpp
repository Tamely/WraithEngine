#include "wpch.h"
#include "Editor/TextureLoader.h"
#include <filesystem>

namespace Wraith {

	std::string TextureLoader::GetTextureKey(const std::string& filePath, bool flipVertically) const {
		// Create unique key that includes flip state
		return filePath + (flipVertically ? "_flipped" : "_normal");
	}

	ImTextureID TextureLoader::LoadTexture(const std::string& filePath, bool flipVertically) {
		std::string textureKey = GetTextureKey(filePath, flipVertically);

		// Check if already loaded with this flip state
		auto it = m_LoadedTextures.find(textureKey);
		if (it != m_LoadedTextures.end()) {
			return reinterpret_cast<ImTextureID>(it->second->GetRendererID());
		}

		// Check if file exists
		if (!std::filesystem::exists(filePath)) {
			W_CORE_WARN("Texture file does not exist: {0}", filePath);
			return GetWhiteTexture();
		}

		try {
			Ref<Texture2D> texture;
			texture = Texture2D::Create(filePath, flipVertically);

			if (texture) {
				m_LoadedTextures[textureKey] = texture;
				m_LoadedTexturesByID[reinterpret_cast<ImTextureID>(texture->GetRendererID())] = texture;
				W_CORE_INFO("Loaded texture: {0} ({1}x{2}) {3}", filePath,
					texture->GetWidth(), texture->GetHeight(),
					flipVertically ? "[Flipped]" : "[Normal]");

				return reinterpret_cast<ImTextureID>(texture->GetRendererID());
			}
			else {
				W_CORE_ERROR("Failed to create texture: {0}", filePath);
				return GetWhiteTexture();
			}
		}
		catch (const std::exception& e) {
			W_CORE_ERROR("Exception loading texture {0}: {1}", filePath, e.what());
			return GetWhiteTexture();
		}
	}

	Ref<Texture2D> TextureLoader::GetTexture(const std::string& filePath, bool flipVertically) {
		std::string textureKey = GetTextureKey(filePath, flipVertically);
		auto it = m_LoadedTextures.find(textureKey);
		return (it != m_LoadedTextures.end()) ? it->second : nullptr;
	}

	bool TextureLoader::IsTextureLoaded(const std::string& filePath, bool flipVertically) const {
		std::string textureKey = GetTextureKey(filePath, flipVertically);
		return m_LoadedTextures.find(textureKey) != m_LoadedTextures.end();
	}

	void TextureLoader::UnloadTexture(const std::string& filePath, bool flipVertically) {
		std::string textureKey = GetTextureKey(filePath, flipVertically);
		auto it = m_LoadedTextures.find(textureKey);
		if (it != m_LoadedTextures.end()) {
			W_CORE_INFO("Unloading texture: {0} {1}", filePath,
				flipVertically ? "[Flipped]" : "[Normal]");

			m_LoadedTexturesByID.erase(reinterpret_cast<ImTextureID*>(it->second->GetRendererID()));
			m_LoadedTextures.erase(it);
		}
	}

	void TextureLoader::UnloadAllTextures() {
		W_CORE_INFO("Unloading all textures ({0} total)", m_LoadedTextures.size());
		m_LoadedTextures.clear();
		m_LoadedTexturesByID.clear();
		m_WhiteTexture = nullptr;
	}

	bool TextureLoader::ReloadTexture(const std::string& filePath, bool flipVertically) {
		UnloadTexture(filePath, flipVertically);
		ImTextureID result = LoadTexture(filePath, flipVertically);
		return result != GetWhiteTexture(); // Success if not fallback texture
	}

	std::pair<uint32_t, uint32_t> TextureLoader::GetTextureDimensions(const std::string& filePath, bool flipVertically) const {
		std::string textureKey = GetTextureKey(filePath, flipVertically);
		auto it = m_LoadedTextures.find(textureKey);
		if (it != m_LoadedTextures.end()) {
			return { it->second->GetWidth(), it->second->GetHeight() };
		}
		return { 0, 0 };
	}

	uint32_t TextureLoader::GetTextureWidth(ImTextureID loadedID) const {
		auto it = m_LoadedTexturesByID.find(loadedID);
		if (it != m_LoadedTexturesByID.end()) {
			return it->second->GetWidth();
		}
		return 0;
	}

	uint32_t TextureLoader::GetTextureHeight(ImTextureID loadedID) const {
		auto it = m_LoadedTexturesByID.find(loadedID);
		if (it != m_LoadedTexturesByID.end()) {
			return it->second->GetHeight();
		}
		return 0;
	}

	ImTextureID TextureLoader::GetWhiteTexture() {
		if (!m_WhiteTexture) {
			// Create a 1x1 white texture as fallback
			m_WhiteTexture = Texture2D::Create(1, 1);
			uint32_t whiteData = 0xFFFFFFFF;
			m_WhiteTexture->SetData(&whiteData, sizeof(uint32_t));
			W_CORE_INFO("Created white fallback texture");
		}
		return reinterpret_cast<ImTextureID>(m_WhiteTexture->GetRendererID());
	}
}
