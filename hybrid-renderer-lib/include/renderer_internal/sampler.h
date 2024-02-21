#pragma once

#include <vulkan/vulkan.h>

#include "renderer_internal/render_context.h"

namespace hri
{
	class ImageSampler
	{
	public:
		ImageSampler(
			RenderContext* ctx,
			VkFilter magFilter = VK_FILTER_NEAREST,
			VkFilter minFilter = VK_FILTER_NEAREST,
			VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
			VkSamplerAddressMode addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			VkSamplerAddressMode addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			VkSamplerAddressMode addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			float mipLodBias = 0.0f,
			float minLod = 0.0f,
			float maxLod = VK_LOD_CLAMP_NONE,
			bool useAnisotropy = false,
			float maxAnisotropy = 0.0f,
			bool compare = false,
			VkCompareOp compareOp = VK_COMPARE_OP_NEVER,
			VkBorderColor borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
			bool unnormalizedCoordinates = false,
			VkSamplerCreateFlags flags = 0
		);

		virtual ~ImageSampler();

		ImageSampler(const ImageSampler&) = delete;
		ImageSampler& operator=(const ImageSampler&) = delete;

		inline VkSampler sampler() const { return m_sampler; }

	private:
		RenderContext* m_pCtx = nullptr;
		VkSampler m_sampler = VK_NULL_HANDLE;
	};
}
