#include "renderer_internal/image.h"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "renderer_internal/render_context.h"

using namespace hri;

ImageResource::ImageResource(
	RenderContext& ctx,
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
	:
	m_ctx(ctx),
	extent(extent),
	imageType(type),
	format(format),
	samples(samples),
	usage(usage)
{
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
	allocationInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
	allocationInfo.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	allocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
	HRI_VK_CHECK(vmaCreateImage(m_ctx.allocator, &imageCreateInfo, &allocationInfo, &image, &m_allocation, nullptr));
}

ImageResource::~ImageResource()
{
	release();
}

ImageResource::ImageResource(ImageResource&& other) noexcept
	:
	m_ctx(other.m_ctx),
	m_allocation(other.m_allocation),
	extent(other.extent),
	imageType(other.imageType),
	format(other.format),
	samples(other.samples),
	usage(other.usage),
	image(other.image),
	view(other.view)
{
	other.image = VK_NULL_HANDLE;
	other.view = VK_NULL_HANDLE;
	other.m_allocation = VK_NULL_HANDLE;
}

ImageResource& ImageResource::operator=(ImageResource&& other) noexcept
{
	if (this == &other)
	{
		return *this;
	}

	release();
	m_ctx = std::move(other.m_ctx);
	m_allocation = other.m_allocation;
	extent = other.extent;
	imageType = other.imageType;
	format = other.format;
	samples = other.samples;
	usage = other.usage;
	image = other.image;
	view = other.view;

	other.image = VK_NULL_HANDLE;
	other.view = VK_NULL_HANDLE;
	other.m_allocation = VK_NULL_HANDLE;

	return *this;
}

VkImageView ImageResource::createView(VkImageViewType viewType, VkComponentMapping components, VkImageSubresourceRange subresourceRange)
{
    if (this->view != VK_NULL_HANDLE)
        destroyView();

	VkImageViewCreateInfo createInfo = VkImageViewCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	createInfo.flags = 0;
	createInfo.image = image;
	createInfo.viewType = viewType;
	createInfo.format = format;
	createInfo.components = components;
	createInfo.subresourceRange = subresourceRange;

	HRI_VK_CHECK(vkCreateImageView(m_ctx.device, &createInfo, nullptr, &this->view));
	return this->view;
}

void ImageResource::destroyView()
{
	vkDestroyImageView(m_ctx.device, this->view, nullptr);
    this->view = VK_NULL_HANDLE;
}

void ImageResource::release()
{
	vmaDestroyImage(m_ctx.allocator, image, m_allocation);
	destroyView();
}
