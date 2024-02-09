#include "renderer_internal/texture.h"

#include <cassert>
#include <cstdint>
#include <cstring>

using namespace hri;

Texture::Texture(TextureExtent extent)
	:
	m_extent(extent),
	m_imageDataSize(0),
	m_pImageData(nullptr)
{
	m_imageDataSize = m_extent.width * m_extent.height * m_extent.channels;
	assert(m_imageDataSize > 0);

	m_pImageData = new uint8_t[m_imageDataSize]{};
}

Texture::Texture(TextureExtent extent, void* data)
	:
	Texture(extent)
{
	assert(data != nullptr);
	memcpy(m_pImageData, data, m_imageDataSize);
}

Texture::~Texture()
{
	delete[] m_pImageData;
}

void Texture::copyToTexture(void* data, size_t size)
{
	assert(data != nullptr);
	assert(size <= m_imageDataSize);
	memcpy(m_pImageData, data, size);
}
