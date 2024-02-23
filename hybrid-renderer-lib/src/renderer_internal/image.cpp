#include "renderer_internal/image.h"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "renderer_internal/render_context.h"

using namespace hri;

ImageResource ImageResource::init(
	RenderContext* ctx,
	VkImageType type,
	VkFormat format,
	VkSampleCountFlagBits samples,
	VkExtent3D extent,
	uint32_t mipLevels,
	uint32_t arrayLayers,
	VkImageUsageFlags usage,
	VkImageTiling tiling,
	VkImageCreateFlags flags,
	VkImageLayout initialLayout
)
{
	assert(ctx != nullptr);

	ImageResource image = ImageResource{};
    image.extent = extent;
	image.format = format;

	VkImageCreateInfo imageCreateInfo = VkImageCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    imageCreateInfo.flags = flags;
    imageCreateInfo.imageType = type;
    imageCreateInfo.format = format;
    imageCreateInfo.extent = extent;
    imageCreateInfo.mipLevels = mipLevels;
    imageCreateInfo.arrayLayers = arrayLayers;
    imageCreateInfo.samples = samples;
    imageCreateInfo.tiling = tiling;
    imageCreateInfo.usage = usage;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.queueFamilyIndexCount = 0;
    imageCreateInfo.pQueueFamilyIndices = nullptr;
    imageCreateInfo.initialLayout = initialLayout;

	VmaAllocationCreateInfo allocationInfo = VmaAllocationCreateInfo{};
	allocationInfo.flags = 0;
	allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
	HRI_VK_CHECK(vmaCreateImage(ctx->allocator, &imageCreateInfo, &allocationInfo, &image.image, &image.allocation, nullptr));

	return image;
}

void ImageResource::destroy(RenderContext* ctx, ImageResource& image)
{
	assert(ctx != nullptr);
	vmaDestroyImage(ctx->allocator, image.image, image.allocation);
    vkDestroyImageView(ctx->device, image.view, nullptr);

	memset(&image, 0, sizeof(ImageResource));
}

VkImageView ImageResource::createView(RenderContext* ctx, VkImageViewType viewType, VkComponentMapping components, VkImageSubresourceRange subresourceRange)
{
	assert(ctx != nullptr);

    if (this->view != VK_NULL_HANDLE)
        destroyView(ctx);

	VkImageViewCreateInfo createInfo = VkImageViewCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	createInfo.flags = 0;
	createInfo.image = image;
	createInfo.viewType = viewType;
	createInfo.format = format;
	createInfo.components = components;
	createInfo.subresourceRange = subresourceRange;

	HRI_VK_CHECK(vkCreateImageView(ctx->device, &createInfo, nullptr, &this->view));
	return this->view;
}

void ImageResource::destroyView(RenderContext* ctx)
{
	assert(ctx != nullptr);
	vkDestroyImageView(ctx->device, this->view, nullptr);
    this->view = VK_NULL_HANDLE;
}
