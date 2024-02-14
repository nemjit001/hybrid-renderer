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

	createRenderPass();
	createFrameResources();

	// Set up builtin shaders
	shaderDB->registerShader(HRI_SHADER_DB_BUILTIN_NAME("PresentVertShader"), Shader::init(m_pCtx, gPresentVertShader, sizeof(uint32_t) * HRI_SIZEOF_ARRAY(gPresentVertShader), VK_SHADER_STAGE_VERTEX_BIT));
	shaderDB->registerShader(HRI_SHADER_DB_BUILTIN_NAME("PresentFragShader"), Shader::init(m_pCtx, gPresentFragShader, sizeof(uint32_t) * HRI_SIZEOF_ARRAY(gPresentFragShader), VK_SHADER_STAGE_FRAGMENT_BIT));
	shaderDB->createPipeline(HRI_SHADER_DB_BUILTIN_NAME("Present"), { HRI_SHADER_DB_BUILTIN_NAME("PresentVertShader"), HRI_SHADER_DB_BUILTIN_NAME("PresentFragShader") }, getPresentPipelineBuilder());

	// Set up builtin nodes
	m_presentNode.setPipeline(shaderDB->getPipeline(HRI_SHADER_DB_BUILTIN_NAME("Present")));
}

FrameGraph::~FrameGraph()
{
	vkDestroyRenderPass(m_pCtx->device, m_renderPass, nullptr);
	destroyFrameResources();
}

void FrameGraph::execute(VkCommandBuffer commandBuffer, uint32_t activeSwapImageIdx) const
{
	assert(activeSwapImageIdx < m_framebuffers.size());
	VkExtent2D swapExtent = m_pCtx->swapchain.extent;

	VkClearValue clearValues[] = {
		VkClearValue{{ 0.0f, 0.0f, 0.0f, 0.0f }},	// Swap attachment
		VkClearValue{{ 1.0f, 0x00 }},				// GBuffer Depth
		VkClearValue{{ 0.0f, 0.0f, 0.0f, 0.0f }},	// GBuffer Normal
		VkClearValue{{ 0.0f, 0.0f, 0.0f, 0.0f }},	// GBuffer Albedo
	};

	VkRenderPassBeginInfo passBeginInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	passBeginInfo.renderPass = m_renderPass;
	passBeginInfo.framebuffer = m_framebuffers[activeSwapImageIdx];
	passBeginInfo.renderArea = VkRect2D{ 0, 0, swapExtent.width, swapExtent.height };
	passBeginInfo.clearValueCount = HRI_SIZEOF_ARRAY(clearValues);
	passBeginInfo.pClearValues = clearValues;
	vkCmdBeginRenderPass(commandBuffer, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	// TODO: execute deferred pass using precompiled shaders
	//		DO: find a way to bind needed buffers -> use scene data?

	vkCmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);

	// TODO: allocate resource descriptors for input attachments
	// TODO: set viewport & scissor state
	m_presentNode.execute(commandBuffer);

	vkCmdEndRenderPass(commandBuffer);
}

void FrameGraph::recreateFrameResources()
{
	destroyFrameResources();
	createFrameResources();
}

void FrameGraph::createRenderPass()
{
	// Set up deferred rendering subpass
	VkAttachmentReference gbufferDepthRef = VkAttachmentReference{ 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
	VkAttachmentDescription gbufferDepth = VkAttachmentDescription{};
	gbufferDepth.flags = 0;
	gbufferDepth.format = VK_FORMAT_D32_SFLOAT;
	gbufferDepth.samples = VK_SAMPLE_COUNT_1_BIT;
	gbufferDepth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	gbufferDepth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	gbufferDepth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	gbufferDepth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
	gbufferDepth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	gbufferDepth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference gbufferNormalRef = VkAttachmentReference{ 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	VkAttachmentDescription gbufferNormal = VkAttachmentDescription{};
	gbufferNormal.flags = 0;
	gbufferNormal.format = VK_FORMAT_R8G8B8A8_SNORM;
	gbufferNormal.samples = VK_SAMPLE_COUNT_1_BIT;
	gbufferNormal.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	gbufferNormal.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	gbufferNormal.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	gbufferNormal.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	gbufferNormal.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	gbufferNormal.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference gbufferAlbedoRef = VkAttachmentReference{ 3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	VkAttachmentDescription gbufferAlbedo = VkAttachmentDescription{};
	gbufferAlbedo.flags = 0;
	gbufferAlbedo.format = VK_FORMAT_R8G8B8A8_UNORM;
	gbufferAlbedo.samples = VK_SAMPLE_COUNT_1_BIT;
	gbufferAlbedo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	gbufferAlbedo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	gbufferAlbedo.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	gbufferAlbedo.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	gbufferAlbedo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	gbufferAlbedo.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference gbufferColorAttachments[] = {
		gbufferNormalRef,
		gbufferAlbedoRef,
	};

	VkSubpassDescription gBufferSubpass = VkSubpassDescription{};
	gBufferSubpass.flags = 0;
	gBufferSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	gBufferSubpass.inputAttachmentCount = 0;
	gBufferSubpass.pInputAttachments = nullptr;
	gBufferSubpass.colorAttachmentCount = HRI_SIZEOF_ARRAY(gbufferColorAttachments);
	gBufferSubpass.pColorAttachments = gbufferColorAttachments;
	gBufferSubpass.pResolveAttachments = nullptr;
	gBufferSubpass.pDepthStencilAttachment = &gbufferDepthRef;
	gBufferSubpass.preserveAttachmentCount = 0;
	gBufferSubpass.pPreserveAttachments = nullptr;

	// Set up presentation subpass
	VkAttachmentReference swapAttachmentColorRef = VkAttachmentReference{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	VkAttachmentDescription swapAttachment = VkAttachmentDescription{};
	swapAttachment.flags = 0;
	swapAttachment.format = m_pCtx->swapchain.image_format;
	swapAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	swapAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	swapAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	swapAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	swapAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	swapAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	swapAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkSubpassDescription presentSubpass = VkSubpassDescription{};
	presentSubpass.flags = 0;
	presentSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	presentSubpass.inputAttachmentCount = 0;
	presentSubpass.pInputAttachments = nullptr;
	presentSubpass.colorAttachmentCount = 1;
	presentSubpass.pColorAttachments = &swapAttachmentColorRef;
	presentSubpass.pResolveAttachments = nullptr;
	presentSubpass.pDepthStencilAttachment = nullptr;
	presentSubpass.preserveAttachmentCount = 0;
	presentSubpass.pPreserveAttachments = nullptr;

	// Set up subpass dependencies
	VkSubpassDependency presentDependency = VkSubpassDependency{};
	presentDependency.srcSubpass = 0;	// gbuffer pass
	presentDependency.dstSubpass = 1;	// present pass
	presentDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	presentDependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	presentDependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	presentDependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	presentDependency.dependencyFlags = 0;

	// Set up render pass
	VkAttachmentDescription attachments[] = {
		swapAttachment,
		gbufferDepth,
		gbufferNormal,
		gbufferAlbedo,
	};

	VkSubpassDescription subpasses[] = {
		gBufferSubpass,
		presentSubpass,
	};

	VkSubpassDependency dependencies[] = {
		presentDependency,
	};

	VkRenderPassCreateInfo renderPassCreateInfo = VkRenderPassCreateInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	renderPassCreateInfo.flags = 0;
	renderPassCreateInfo.attachmentCount = HRI_SIZEOF_ARRAY(attachments);
	renderPassCreateInfo.pAttachments = attachments;
	renderPassCreateInfo.subpassCount = HRI_SIZEOF_ARRAY(subpasses);
	renderPassCreateInfo.pSubpasses = subpasses;
	renderPassCreateInfo.dependencyCount = HRI_SIZEOF_ARRAY(dependencies);
	renderPassCreateInfo.pDependencies = dependencies;
	HRI_VK_CHECK(vkCreateRenderPass(m_pCtx->device, &renderPassCreateInfo, nullptr, &m_renderPass));
}

void FrameGraph::createFrameResources()
{
	VkExtent2D swapExtent = m_pCtx->swapchain.extent;

	m_gbufferDepthTarget = RenderTarget::init(
		m_pCtx,
		VK_FORMAT_D32_SFLOAT,
		swapExtent,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_IMAGE_ASPECT_DEPTH_BIT
	);

	m_gbufferNormalTarget = RenderTarget::init(
		m_pCtx,
		VK_FORMAT_R8G8B8A8_SNORM,
		swapExtent,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT
	);

	m_gbufferAlbedoTarget = RenderTarget::init(
		m_pCtx,
		VK_FORMAT_R8G8B8A8_UNORM,
		swapExtent,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT
	);

	m_swapViews = m_pCtx->swapchain.get_image_views().value();
	for (auto const& swapView : m_swapViews)
	{
		VkImageView fbAttachments[] = {
			swapView,
			m_gbufferDepthTarget.view,
			m_gbufferNormalTarget.view,
			m_gbufferAlbedoTarget.view,
		};

		VkFramebufferCreateInfo fbCreateInfo = VkFramebufferCreateInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
		fbCreateInfo.flags = 0;
		fbCreateInfo.attachmentCount = HRI_SIZEOF_ARRAY(fbAttachments);
		fbCreateInfo.pAttachments = fbAttachments;
		fbCreateInfo.renderPass = m_renderPass;
		fbCreateInfo.width = swapExtent.width;
		fbCreateInfo.height = swapExtent.height;
		fbCreateInfo.layers = 1;

		VkFramebuffer framebuffer = VK_NULL_HANDLE;
		HRI_VK_CHECK(vkCreateFramebuffer(m_pCtx->device, &fbCreateInfo, nullptr, &framebuffer));
		m_framebuffers.push_back(framebuffer);
	}
}

void FrameGraph::destroyFrameResources()
{
	RenderTarget::destroy(m_pCtx, m_gbufferDepthTarget);
	RenderTarget::destroy(m_pCtx, m_gbufferNormalTarget);
	RenderTarget::destroy(m_pCtx, m_gbufferAlbedoTarget);

	m_pCtx->swapchain.destroy_image_views(m_swapViews);
	for (auto const& framebuffer : m_framebuffers)
	{
		vkDestroyFramebuffer(m_pCtx->device, framebuffer, nullptr);
	}

	m_framebuffers.clear();
}

GraphicsPipelineBuilder FrameGraph::getPresentPipelineBuilder() const
{
	static const std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments = {
		VkPipelineColorBlendAttachmentState{
			VK_FALSE,
			VK_BLEND_FACTOR_ONE,
			VK_BLEND_FACTOR_ZERO,
			VK_BLEND_OP_ADD,
			VK_BLEND_FACTOR_ONE,
			VK_BLEND_FACTOR_ZERO,
			VK_BLEND_OP_ADD,
			VK_COLOR_COMPONENT_R_BIT
			| VK_COLOR_COMPONENT_G_BIT
			| VK_COLOR_COMPONENT_B_BIT
			| VK_COLOR_COMPONENT_A_BIT
		}
	};

	static const GraphicsPipelineBuilder presentPipelineBuilder = GraphicsPipelineBuilder{
		{}, {},	// No vertex bindings & attributes
		GraphicsPipelineBuilder::initInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE),
		GraphicsPipelineBuilder::initDefaultViewport(1.0f, 1.0f),
		GraphicsPipelineBuilder::initDefaultScissor(1, 1),
		GraphicsPipelineBuilder::initRasterizationState(VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE),
		GraphicsPipelineBuilder::initMultisampleState(VK_SAMPLE_COUNT_1_BIT),
		GraphicsPipelineBuilder::initDepthStencilState(VK_FALSE, VK_FALSE, VK_COMPARE_OP_NEVER),
		GraphicsPipelineBuilder::initColorBlendState(colorBlendAttachments),
		{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR },
		m_renderPass, 1
	};

	return presentPipelineBuilder;
}
