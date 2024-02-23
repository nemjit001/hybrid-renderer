#pragma once

#include <cstddef>
#include <cstdint>

namespace hri
{
	/// @brief The TextureExtent represents a texture's width, height, and channels
	struct TextureExtent
	{
		uint32_t width;
		uint32_t height;
		uint32_t channels;
	};

	/// @brief A Texture represents tightly packed image data.
	class Texture
	{
	public:
		/// @brief Create an empty texture.
		/// @param extent The texture's extent.
		Texture(TextureExtent extent);

		/// @brief Create a prefilled texture object.
		/// @param extent The texture's extent. MUST match the data pointer's size.
		/// @param data A pointer to the image data to copy.
		Texture(TextureExtent extent, void* data);

		/// @brief Destroy this texture object.
		virtual ~Texture();

		/// @brief Copy data into this texture object.
		/// @param data A pointer to the image data to copy.
		/// @param size The image data size. MUST be less than or equal to the image's size.
		void copyToTexture(void* data, size_t size);

	private:
		TextureExtent m_extent	= TextureExtent{};
		size_t m_imageDataSize	= 0;
		uint8_t* m_pImageData	= nullptr;
	};
}
