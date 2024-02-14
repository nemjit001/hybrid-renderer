#include "renderer_internal/frame_graph.h"

#include <cassert>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "renderer_internal/render_context.h"

using namespace hri;

RenderTarget RenderTarget::init(
	RenderContext* ctx,
	VkFormat format,
	VkExtent2D extent,
	VkImageUsageFlags usage,
	VkImageAspectFlags imageAspect
)
{
	assert(ctx != nullptr);
	RenderTarget target = RenderTarget{};

	VkImageCreateInfo imageCreateInfo = VkImageCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	imageCreateInfo.flags = 0;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = format;
	imageCreateInfo.extent = VkExtent3D{ extent.width, extent.height, 1 };
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.usage = usage;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.queueFamilyIndexCount = 0;
	imageCreateInfo.pQueueFamilyIndices = nullptr;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VmaAllocationCreateInfo imageAllocationInfo = VmaAllocationCreateInfo{};
	imageAllocationInfo.flags = 0;
	imageAllocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
	HRI_VK_CHECK(vmaCreateImage(ctx->allocator, &imageCreateInfo, &imageAllocationInfo, &target.image, &target.allocation, nullptr));

	VkImageViewCreateInfo viewCreateInfo = VkImageViewCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	viewCreateInfo.flags = 0;
	viewCreateInfo.image = target.image;
	viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewCreateInfo.format = format;
	viewCreateInfo.components = VkComponentMapping{
		VK_COMPONENT_SWIZZLE_IDENTITY,
		VK_COMPONENT_SWIZZLE_IDENTITY,
		VK_COMPONENT_SWIZZLE_IDENTITY,
		VK_COMPONENT_SWIZZLE_IDENTITY,
	};
	viewCreateInfo.subresourceRange = VkImageSubresourceRange{
		imageAspect,
		0, 1,
		0, 1,
	};
	HRI_VK_CHECK(vkCreateImageView(ctx->device, &viewCreateInfo, nullptr, &target.view));

	return target;
}

void RenderTarget::destroy(RenderContext* ctx, RenderTarget& renderTarget)
{
	assert(ctx != nullptr);

	vmaDestroyImage(ctx->allocator, renderTarget.image, renderTarget.allocation);
	vkDestroyImageView(ctx->device, renderTarget.view, nullptr);

	memset(&renderTarget, 0, sizeof(RenderTarget));
}

void RasterFrameGraphNode::execute(VkCommandBuffer commandBuffer) const
{
	assert(m_pPSO != nullptr);

	setDynamicViewportState(commandBuffer);

	// TODO: bind resources
	vkCmdBindPipeline(commandBuffer, m_pPSO->bindPoint, m_pPSO->pipeline);
	// TODO: bind buffers
	// TODO: draw
}

void RasterFrameGraphNode::setViewport(Viewport viewport)
{
	m_viewport = viewport;
}

void RasterFrameGraphNode::setScissor(Scissor scissor)
{
	m_scissor = scissor;
}

void RasterFrameGraphNode::setDynamicViewportState(VkCommandBuffer commandBuffer) const
{
	VkViewport viewport = VkViewport{
		m_viewport.x, m_viewport.y,
		m_viewport.width, m_viewport.height,
		m_viewport.minDepth, m_viewport.maxDepth,
	};

	VkRect2D scissor = VkRect2D{
		m_scissor.x, m_scissor.y,
		m_scissor.width, m_scissor.height,
	};

	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}

void PresentFrameGraphNode::execute(VkCommandBuffer commandBuffer) const
{
	assert(m_pPSO != nullptr);

	setDynamicViewportState(commandBuffer);

	// TODO: bind resources
	vkCmdBindPipeline(commandBuffer, m_pPSO->bindPoint, m_pPSO->pipeline);
	vkCmdDraw(commandBuffer, 3, 1, 0, 0);
}

FrameGraph::FrameGraph(RenderContext* ctx)
	:
	m_pCtx(ctx)
{
	assert(ctx != nullptr);

	createFrameGraphResources();
}

FrameGraph::~FrameGraph()
{
	destroyFrameGraphResources();
}

void FrameGraph::execute(VkCommandBuffer commandBuffer, uint32_t activeSwapImageIdx) const
{
	assert(activeSwapImageIdx < m_framebuffers.size());
	VkExtent2D swapExtent = m_pCtx->swapchain.extent;

	//VkClearValue clearValues[] = {
	//	VkClearValue{{ 0.0f, 0.0f, 0.0f, 0.0f }},	// Swap attachment
	//	VkClearValue{{ 1.0f, 0x00 }},				// GBuffer Depth
	//	VkClearValue{{ 0.0f, 0.0f, 0.0f, 0.0f }},	// GBuffer Normal
	//	VkClearValue{{ 0.0f, 0.0f, 0.0f, 0.0f }},	// GBuffer Albedo
	//};

	//VkRenderPassBeginInfo passBeginInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	//passBeginInfo.renderPass = VK_NULL_HANDLE;
	//passBeginInfo.framebuffer = m_framebuffers[activeSwapImageIdx];
	//passBeginInfo.renderArea = VkRect2D{ 0, 0, swapExtent.width, swapExtent.height };
	//passBeginInfo.clearValueCount = HRI_SIZEOF_ARRAY(clearValues);
	//passBeginInfo.pClearValues = clearValues;
	//vkCmdBeginRenderPass(commandBuffer, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	//// TODO: execute generated frame graph

	//vkCmdEndRenderPass(commandBuffer);
}

void FrameGraph::generateGraph()
{
	destroyFrameGraphResources();

	// TODO: generate render pass based on topological sort of graph nodes

	createFrameGraphResources();
}

void FrameGraph::createFrameGraphResources()
{
	VkExtent2D swapExtent = m_pCtx->swapchain.extent;

	m_swapViews = m_pCtx->swapchain.get_image_views().value();
	for (auto const& swapView : m_swapViews)
	{
		VkImageView fbAttachments[] = {
			swapView,
			// TODO: add auto generated attachments
		};

		VkFramebufferCreateInfo fbCreateInfo = VkFramebufferCreateInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
		fbCreateInfo.flags = 0;
		fbCreateInfo.attachmentCount = HRI_SIZEOF_ARRAY(fbAttachments);
		fbCreateInfo.pAttachments = fbAttachments;
		fbCreateInfo.renderPass = VK_NULL_HANDLE;
		fbCreateInfo.width = swapExtent.width;
		fbCreateInfo.height = swapExtent.height;
		fbCreateInfo.layers = 1;

		VkFramebuffer framebuffer = VK_NULL_HANDLE;
		HRI_VK_CHECK(vkCreateFramebuffer(m_pCtx->device, &fbCreateInfo, nullptr, &framebuffer));
		m_framebuffers.push_back(framebuffer);
	}
}

void FrameGraph::destroyFrameGraphResources()
{
	m_pCtx->swapchain.destroy_image_views(m_swapViews);
	for (auto const& framebuffer : m_framebuffers)
	{
		vkDestroyFramebuffer(m_pCtx->device, framebuffer, nullptr);
	}

	m_framebuffers.clear();
}
