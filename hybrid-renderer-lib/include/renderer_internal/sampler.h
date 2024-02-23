#pragma once

#include <vulkan/vulkan.h>

#include "renderer_internal/render_context.h"

namespace hri
{
	/// @brief Image Samplers are used to sample images in shader programs.
	class ImageSampler
	{
	public:
		/// @brief Create a new image sampler.
		/// @param ctx Render Context to use.
		/// @param magFilter 
		/// @param minFilter 
		/// @param mipmapMode 
		/// @param addressModeU 
		/// @param addressModeV 
		/// @param addressModeW 
		/// @param mipLodBias 
		/// @param minLod 
		/// @param maxLod 
		/// @param useAnisotropy 
		/// @param maxAnisotropy 
		/// @param compare 
		/// @param compareOp 
		/// @param borderColor 
		/// @param unnormalizedCoordinates 
		/// @param flags 
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

		/// @brief Destroy this image sampler instance.
		virtual ~ImageSampler();

		// Disallow copy behaviour
		ImageSampler(const ImageSampler&) = delete;
		ImageSampler& operator=(const ImageSampler&) = delete;

	public:
		VkSampler sampler = VK_NULL_HANDLE;

	private:
		RenderContext* m_pCtx = nullptr;
	};
}
