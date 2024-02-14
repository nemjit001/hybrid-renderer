#include "renderer_internal/frame_graph.h"

#include <cassert>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "renderer_internal/render_context.h"

using namespace hri;

#define HRI_SHADER_DB_BUILTIN_NAME(name)	("Builtin::" ## name)

static const uint32_t gPresentVertShader[] = {
	0x07230203,0x00010000,0x0008000b,0x0000002b,0x00000000,0x00020011,0x00000001,0x0006000b,
	0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
	0x0008000f,0x00000000,0x00000004,0x6e69616d,0x00000000,0x00000009,0x0000000c,0x0000001d,
	0x00030003,0x00000002,0x000001c2,0x00040005,0x00000004,0x6e69616d,0x00000000,0x00050005,
	0x00000009,0x65726353,0x56556e65,0x00000000,0x00060005,0x0000000c,0x565f6c67,0x65747265,
	0x646e4978,0x00007865,0x00060005,0x0000001b,0x505f6c67,0x65567265,0x78657472,0x00000000,
	0x00060006,0x0000001b,0x00000000,0x505f6c67,0x7469736f,0x006e6f69,0x00070006,0x0000001b,
	0x00000001,0x505f6c67,0x746e696f,0x657a6953,0x00000000,0x00070006,0x0000001b,0x00000002,
	0x435f6c67,0x4470696c,0x61747369,0x0065636e,0x00070006,0x0000001b,0x00000003,0x435f6c67,
	0x446c6c75,0x61747369,0x0065636e,0x00030005,0x0000001d,0x00000000,0x00040047,0x00000009,
	0x0000001e,0x00000000,0x00040047,0x0000000c,0x0000000b,0x0000002a,0x00050048,0x0000001b,
	0x00000000,0x0000000b,0x00000000,0x00050048,0x0000001b,0x00000001,0x0000000b,0x00000001,
	0x00050048,0x0000001b,0x00000002,0x0000000b,0x00000003,0x00050048,0x0000001b,0x00000003,
	0x0000000b,0x00000004,0x00030047,0x0000001b,0x00000002,0x00020013,0x00000002,0x00030021,
	0x00000003,0x00000002,0x00030016,0x00000006,0x00000020,0x00040017,0x00000007,0x00000006,
	0x00000002,0x00040020,0x00000008,0x00000003,0x00000007,0x0004003b,0x00000008,0x00000009,
	0x00000003,0x00040015,0x0000000a,0x00000020,0x00000001,0x00040020,0x0000000b,0x00000001,
	0x0000000a,0x0004003b,0x0000000b,0x0000000c,0x00000001,0x0004002b,0x0000000a,0x0000000e,
	0x00000001,0x0004002b,0x0000000a,0x00000010,0x00000002,0x00040017,0x00000017,0x00000006,
	0x00000004,0x00040015,0x00000018,0x00000020,0x00000000,0x0004002b,0x00000018,0x00000019,
	0x00000001,0x0004001c,0x0000001a,0x00000006,0x00000019,0x0006001e,0x0000001b,0x00000017,
	0x00000006,0x0000001a,0x0000001a,0x00040020,0x0000001c,0x00000003,0x0000001b,0x0004003b,
	0x0000001c,0x0000001d,0x00000003,0x0004002b,0x0000000a,0x0000001e,0x00000000,0x0004002b,
	0x00000006,0x0000001f,0x40000000,0x0004002b,0x00000006,0x00000022,0x3f800000,0x0004002b,
	0x00000006,0x00000025,0x00000000,0x00040020,0x00000029,0x00000003,0x00000017,0x00050036,
	0x00000002,0x00000004,0x00000000,0x00000003,0x000200f8,0x00000005,0x0004003d,0x0000000a,
	0x0000000d,0x0000000c,0x000500c4,0x0000000a,0x0000000f,0x0000000d,0x0000000e,0x000500c7,
	0x0000000a,0x00000011,0x0000000f,0x00000010,0x0004006f,0x00000006,0x00000012,0x00000011,
	0x0004003d,0x0000000a,0x00000013,0x0000000c,0x000500c7,0x0000000a,0x00000014,0x00000013,
	0x00000010,0x0004006f,0x00000006,0x00000015,0x00000014,0x00050050,0x00000007,0x00000016,
	0x00000012,0x00000015,0x0003003e,0x00000009,0x00000016,0x0004003d,0x00000007,0x00000020,
	0x00000009,0x0005008e,0x00000007,0x00000021,0x00000020,0x0000001f,0x00050050,0x00000007,
	0x00000023,0x00000022,0x00000022,0x00050083,0x00000007,0x00000024,0x00000021,0x00000023,
	0x00050051,0x00000006,0x00000026,0x00000024,0x00000000,0x00050051,0x00000006,0x00000027,
	0x00000024,0x00000001,0x00070050,0x00000017,0x00000028,0x00000026,0x00000027,0x00000025,
	0x00000022,0x00050041,0x00000029,0x0000002a,0x0000001d,0x0000001e,0x0003003e,0x0000002a,
	0x00000028,0x000100fd,0x00010038
};

static const uint32_t gPresentFragShader[] = {
	0x07230203,0x00010000,0x0008000b,0x00000014,0x00000000,0x00020011,0x00000001,0x0006000b,
	0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
	0x0007000f,0x00000004,0x00000004,0x6e69616d,0x00000000,0x00000009,0x00000011,0x00030010,
	0x00000004,0x00000007,0x00030003,0x00000002,0x000001c2,0x00040005,0x00000004,0x6e69616d,
	0x00000000,0x00050005,0x00000009,0x67617246,0x6f6c6f43,0x00000072,0x00060005,0x0000000d,
	0x646e6552,0x65527265,0x746c7573,0x00000000,0x00050005,0x00000011,0x65726353,0x56556e65,
	0x00000000,0x00040047,0x00000009,0x0000001e,0x00000000,0x00040047,0x0000000d,0x00000022,
	0x00000000,0x00040047,0x0000000d,0x00000021,0x00000000,0x00040047,0x00000011,0x0000001e,
	0x00000000,0x00020013,0x00000002,0x00030021,0x00000003,0x00000002,0x00030016,0x00000006,
	0x00000020,0x00040017,0x00000007,0x00000006,0x00000004,0x00040020,0x00000008,0x00000003,
	0x00000007,0x0004003b,0x00000008,0x00000009,0x00000003,0x00090019,0x0000000a,0x00000006,
	0x00000001,0x00000000,0x00000000,0x00000000,0x00000001,0x00000000,0x0003001b,0x0000000b,
	0x0000000a,0x00040020,0x0000000c,0x00000000,0x0000000b,0x0004003b,0x0000000c,0x0000000d,
	0x00000000,0x00040017,0x0000000f,0x00000006,0x00000002,0x00040020,0x00000010,0x00000001,
	0x0000000f,0x0004003b,0x00000010,0x00000011,0x00000001,0x00050036,0x00000002,0x00000004,
	0x00000000,0x00000003,0x000200f8,0x00000005,0x0004003d,0x0000000b,0x0000000e,0x0000000d,
	0x0004003d,0x0000000f,0x00000012,0x00000011,0x00050057,0x00000007,0x00000013,0x0000000e,
	0x00000012,0x0003003e,0x00000009,0x00000013,0x000100fd,0x00010038
};

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

FrameGraph::FrameGraph(RenderContext* ctx, ShaderDatabase* shaderDB)
	:
	m_pCtx(ctx)
{
	assert(ctx != nullptr);
	assert(shaderDB != nullptr);

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

void FrameGraph::recreateFrameGraphResources()
{
	destroyFrameGraphResources();
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
