#include "renderer_internal/sampler.h"

#include <vulkan/vulkan.h>

#include "renderer_internal/render_context.h"

using namespace hri;

ImageSampler::ImageSampler(
	RenderContext& ctx,
	VkFilter magFilter,
	VkFilter minFilter,
	VkSamplerMipmapMode mipmapMode,
	VkSamplerAddressMode addressModeU,
	VkSamplerAddressMode addressModeV,
	VkSamplerAddressMode addressModeW,
	float mipLodBias,
	float minLod,
	float maxLod,
	bool useAnisotropy,
	float maxAnisotropy,
	bool compare,
	VkCompareOp compareOp,
	VkBorderColor borderColor,
	bool unnormalizedCoordinates,
	VkSamplerCreateFlags flags
)
	:
	m_ctx(ctx)
{
	VkSamplerCreateInfo createInfo = VkSamplerCreateInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	createInfo.flags = flags;
	createInfo.magFilter = magFilter;
	createInfo.minFilter = minFilter;
	createInfo.mipmapMode = mipmapMode;
	createInfo.addressModeU = addressModeU;
	createInfo.addressModeV = addressModeV;
	createInfo.addressModeW = addressModeW;
	createInfo.mipLodBias = mipLodBias;
	createInfo.anisotropyEnable = useAnisotropy;
	createInfo.maxAnisotropy = maxAnisotropy;
	createInfo.compareEnable = compare;
	createInfo.compareOp = compareOp;
	createInfo.minLod = minLod;
	createInfo.maxLod = maxLod;
	createInfo.borderColor = borderColor;
	createInfo.unnormalizedCoordinates = unnormalizedCoordinates;
	HRI_VK_CHECK(vkCreateSampler(m_ctx.device, &createInfo, nullptr, &sampler));
}

ImageSampler::~ImageSampler()
{
	vkDestroySampler(m_ctx.device, sampler, nullptr);
}
