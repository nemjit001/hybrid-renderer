#include "renderer_internal/render_pass.h"

#include <optional>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "renderer_internal/shader_database.h"

using namespace hri;

RenderPassBuilder::RenderPassBuilder(RenderContext& ctx)
	:
	m_ctx(ctx)
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

VkRenderPass RenderPassBuilder::build()
{
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
	HRI_VK_CHECK(vkCreateRenderPass(m_ctx.device, &createInfo, nullptr, &newPass));

	return newPass;
}

IRenderPassResourceManagerBase::IRenderPassResourceManagerBase(
	RenderContext& ctx,
	VkRenderPass renderPass,
	const std::vector<RenderAttachmentConfig>& attachmentConfigs
)
	:
	m_ctx(ctx),
	m_renderPass(renderPass),
	m_attachmentConfigs(attachmentConfigs)
{
	//
}

IRenderPassResourceManagerBase::~IRenderPassResourceManagerBase()
{
	vkDestroyRenderPass(m_ctx.device, m_renderPass, nullptr);
}

void IRenderPassResourceManagerBase::endRenderPass(ActiveFrame& frame) const
{
	vkCmdEndRenderPass(frame.commandBuffer);
}

void IRenderPassResourceManagerBase::setClearValue(uint32_t attachmentIndex, VkClearValue clearValue)
{
	if (attachmentIndex >= m_clearValues.size())
	{
		m_clearValues.resize(attachmentIndex + 1, VkClearValue{{}});
	}

	m_clearValues[attachmentIndex] = clearValue;
}

void IRenderPassResourceManagerBase::createResources()
{
	m_renderExtent = getRenderExtent();

	for (auto const& config : m_attachmentConfigs)
	{
		ImageResource attachment = ImageResource(
			m_ctx,
			VK_IMAGE_TYPE_2D,
			config.format,
			config.samples,
			VkExtent3D{ m_renderExtent.width, m_renderExtent.height, 1 },
			1, 1,
			config.usage
		);

		attachment.createView(
			VK_IMAGE_VIEW_TYPE_2D,
			VkComponentMapping{
				VK_COMPONENT_SWIZZLE_IDENTITY,
				VK_COMPONENT_SWIZZLE_IDENTITY,
				VK_COMPONENT_SWIZZLE_IDENTITY,
				VK_COMPONENT_SWIZZLE_IDENTITY,
			},
			VkImageSubresourceRange{
				config.aspect,
				0, 1,
				0, 1
			}
		);

		m_imageResources.push_back(std::move(attachment));
	}
}

void IRenderPassResourceManagerBase::destroyResources()
{
	for (auto& attachment : m_imageResources)
	{
		attachment.destroyView();
	}

	m_imageResources.clear();
}

std::vector<VkImageView> IRenderPassResourceManagerBase::getImageResourceViews()
{
	std::vector<VkImageView> views; views.reserve(m_imageResources.size());

	for (auto const& image : m_imageResources)
	{
		views.push_back(image.view);
	}

	return views;
}

VkExtent2D IRenderPassResourceManagerBase::getRenderExtent() const
{
	return m_ctx.swapchain.extent;
}

SwapchainPassResourceManager::SwapchainPassResourceManager(
	RenderContext& ctx,
	VkRenderPass renderPass,
	const std::vector<RenderAttachmentConfig>& attachmentConfigs
)
	:
	hri::IRenderPassResourceManagerBase(ctx, renderPass, attachmentConfigs)
{
	createResources();
}

SwapchainPassResourceManager::~SwapchainPassResourceManager()
{
	destroyResources();
}

void SwapchainPassResourceManager::beginRenderPass(ActiveFrame& frame) const
{
	assert(frame.activeSwapImageIndex < m_swapFramebuffers.size());

	VkRenderPassBeginInfo passBeginInfo = VkRenderPassBeginInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	passBeginInfo.renderPass = m_renderPass;
	passBeginInfo.framebuffer = m_swapFramebuffers[frame.activeSwapImageIndex];
	passBeginInfo.renderArea = VkRect2D{ VkOffset2D{ 0, 0 }, m_renderExtent };
	passBeginInfo.clearValueCount = static_cast<uint32_t>(m_clearValues.size());
	passBeginInfo.pClearValues = m_clearValues.data();
	vkCmdBeginRenderPass(frame.commandBuffer, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void SwapchainPassResourceManager::createResources()
{
	IRenderPassResourceManagerBase::createResources();
	m_swapViews = m_ctx.swapchain.get_image_views().value();

	std::vector<VkImageView> resourceViews = getImageResourceViews();
	for (auto const& swapView : m_swapViews)
	{
		std::vector<VkImageView> attachments = { swapView };
		attachments.insert(attachments.end(), resourceViews.begin(), resourceViews.end());

		VkFramebufferCreateInfo fbCreateInfo = VkFramebufferCreateInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
		fbCreateInfo.flags = 0;
		fbCreateInfo.renderPass = m_renderPass;
		fbCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		fbCreateInfo.pAttachments = attachments.data();
		fbCreateInfo.width = m_renderExtent.width;
		fbCreateInfo.height = m_renderExtent.height;
		fbCreateInfo.layers = 1;

		VkFramebuffer framebuffer = VK_NULL_HANDLE;
		HRI_VK_CHECK(vkCreateFramebuffer(m_ctx.device, &fbCreateInfo, nullptr, &framebuffer));
		m_swapFramebuffers.push_back(framebuffer);
	}
}

void SwapchainPassResourceManager::destroyResources()
{
	IRenderPassResourceManagerBase::destroyResources();

	m_ctx.swapchain.destroy_image_views(m_swapViews);

	for (auto const& framebuffer : m_swapFramebuffers)
	{
		vkDestroyFramebuffer(m_ctx.device, framebuffer, nullptr);
	}

	m_swapViews.clear();
	m_swapFramebuffers.clear();
}

RenderPassResourceManager::RenderPassResourceManager(
	RenderContext& ctx,
	VkRenderPass renderPass,
	const std::vector<RenderAttachmentConfig>& attachmentConfigs
)
	:
	hri::IRenderPassResourceManagerBase(ctx, renderPass, attachmentConfigs)
{
	createResources();
}

RenderPassResourceManager::~RenderPassResourceManager()
{
	destroyResources();
}

void RenderPassResourceManager::beginRenderPass(ActiveFrame& frame) const
{
	VkRenderPassBeginInfo passBeginInfo = VkRenderPassBeginInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	passBeginInfo.renderPass = m_renderPass;
	passBeginInfo.framebuffer = m_framebuffer;
	passBeginInfo.renderArea = VkRect2D{ VkOffset2D{ 0, 0 }, m_renderExtent };
	passBeginInfo.clearValueCount = static_cast<uint32_t>(m_clearValues.size());
	passBeginInfo.pClearValues = m_clearValues.data();
	vkCmdBeginRenderPass(frame.commandBuffer, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void RenderPassResourceManager::createResources()
{
	IRenderPassResourceManagerBase::createResources();

	std::vector<VkImageView> attachments = getImageResourceViews();
	VkFramebufferCreateInfo fbCreateInfo = VkFramebufferCreateInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	fbCreateInfo.flags = 0;
	fbCreateInfo.renderPass = m_renderPass;
	fbCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	fbCreateInfo.pAttachments = attachments.data();
	fbCreateInfo.width = m_renderExtent.width;
	fbCreateInfo.height = m_renderExtent.height;
	fbCreateInfo.layers = 1;
	HRI_VK_CHECK(vkCreateFramebuffer(m_ctx.device, &fbCreateInfo, nullptr, &m_framebuffer));
}

void RenderPassResourceManager::destroyResources()
{
	IRenderPassResourceManagerBase::destroyResources();
	vkDestroyFramebuffer(m_ctx.device, m_framebuffer, nullptr);
}
