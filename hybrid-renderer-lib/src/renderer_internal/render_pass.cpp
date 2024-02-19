#include "renderer_internal/render_pass.h"

#include <optional>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "renderer_internal/shader_database.h"

using namespace hri;

RenderAttachment RenderAttachment::init(RenderContext* ctx, VkImageCreateInfo* createInfo)
{
	assert(ctx != nullptr);
	assert(createInfo != nullptr);

	RenderAttachment attachment = RenderAttachment{};
	attachment.format = createInfo->format;

	VmaAllocationCreateInfo allocationInfo = VmaAllocationCreateInfo{};
	allocationInfo.flags = 0;
	allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
	HRI_VK_CHECK(vmaCreateImage(ctx->allocator, createInfo, &allocationInfo, &attachment.image, &attachment.allocation, nullptr));

	return attachment;
}

void RenderAttachment::destroy(RenderContext* ctx, RenderAttachment& attachment)
{
	assert(ctx != nullptr);
	vmaDestroyImage(ctx->allocator, attachment.image, attachment.allocation);

	memset(&attachment, 0, sizeof(RenderAttachment));
}

VkImageView RenderAttachment::createView(RenderContext* ctx, VkImageViewType viewType, VkComponentMapping components, VkImageSubresourceRange subresourceRange)
{
	assert(ctx != nullptr);

	VkImageViewCreateInfo createInfo = VkImageViewCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	createInfo.flags = 0;
	createInfo.image = image;
	createInfo.viewType = viewType;
	createInfo.format = format;
	createInfo.components = components;
	createInfo.subresourceRange = subresourceRange;

	VkImageView view = VK_NULL_HANDLE;
	HRI_VK_CHECK(vkCreateImageView(ctx->device, &createInfo, nullptr, &view));
	return view;
}

void RenderAttachment::destroyView(RenderContext* ctx, VkImageView view)
{
	assert(ctx != nullptr);
	vkDestroyImageView(ctx->device, view, nullptr);
}

RenderPassBuilder::RenderPassBuilder(RenderContext* ctx)
	:
	m_pCtx(ctx)
{
	// Start first subpass
	this->nextSubpass();
}

RenderPassBuilder& RenderPassBuilder::addAttachment(
	VkFormat format,
	VkSampleCountFlagBits samples,
	VkImageLayout finalLayout,
	VkAttachmentLoadOp loadOp,
	VkAttachmentStoreOp storeOp,
	VkAttachmentLoadOp stencilLoadOp,
	VkAttachmentStoreOp stencilStoreOp,
	VkImageLayout initialLayout
)
{
	m_attachments.push_back(VkAttachmentDescription{
		0,
		format,
		samples,
		loadOp, storeOp,
		stencilLoadOp, stencilStoreOp,
		initialLayout,
		finalLayout
	});

	return *this;
}

RenderPassBuilder& RenderPassBuilder::nextSubpass()
{
	m_subpasses.push_back(SubpassData{});

	return *this;
}

RenderPassBuilder& RenderPassBuilder::setAttachmentReference(AttachmentType type, VkAttachmentReference ref)
{
	SubpassData& currentSubpass = m_subpasses.back();
	
	switch (type)
	{
	case hri::AttachmentType::Color:
		currentSubpass.colorAttachments.push_back(ref);
		break;
	case hri::AttachmentType::DepthStencil:
		currentSubpass.depthStencilAttachment = ref;
		break;
	default:
		break;
	}

	return *this;
}

RenderPassBuilder& RenderPassBuilder::setContext(RenderContext* ctx)
{
	m_pCtx = ctx;
	return *this;
}

VkRenderPass RenderPassBuilder::build()
{
	assert(m_pCtx != nullptr);

	std::vector<VkSubpassDescription> subpasses = {}; subpasses.reserve(m_subpasses.size());
	for (auto const& pass : m_subpasses)
	{
		VkSubpassDescription description = VkSubpassDescription{};
		description.flags = 0;
		description.colorAttachmentCount = static_cast<uint32_t>(pass.colorAttachments.size());
		description.pColorAttachments = pass.colorAttachments.data();
		description.pDepthStencilAttachment = pass.depthStencilAttachment.has_value() ? &pass.depthStencilAttachment.value() : nullptr;

		subpasses.push_back(description);
	}

	VkRenderPassCreateInfo createInfo = VkRenderPassCreateInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	createInfo.flags = 0;
	createInfo.attachmentCount = static_cast<uint32_t>(m_attachments.size());
	createInfo.pAttachments = m_attachments.data();
	createInfo.subpassCount = static_cast<uint32_t>(subpasses.size());
	createInfo.pSubpasses = subpasses.data();
	createInfo.dependencyCount = 0;
	createInfo.pDependencies = nullptr;

	VkRenderPass newPass = VK_NULL_HANDLE;
	HRI_VK_CHECK(vkCreateRenderPass(m_pCtx->device, &createInfo, nullptr, &newPass));

	return newPass;
}
