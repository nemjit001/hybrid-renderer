#include "renderer_internal/render_pass.h"

#include <vector>
#include <vulkan/vulkan.h>

#include "renderer_internal/shader_database.h"

using namespace hri;

static const uint32_t gGlslPresentVertShader[] = {
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

static const uint32_t gGlslPresentFragShader[] = {
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

BuiltinRenderPass::BuiltinRenderPass(RenderContext* ctx, ShaderDatabase* shaderDB)
	:
	m_pCtx(ctx)
{
	assert(m_pCtx != nullptr);

	VkAttachmentReference swapRef = VkAttachmentReference{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
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

	VkSubpassDescription subpass = VkSubpassDescription{};
	subpass.flags = 0;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &swapRef;

	VkRenderPassCreateInfo passCreateInfo = VkRenderPassCreateInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	passCreateInfo.flags = 0;
	passCreateInfo.attachmentCount = 1;
	passCreateInfo.pAttachments = &swapAttachment;
	passCreateInfo.subpassCount = 1;
	passCreateInfo.pSubpasses = &subpass;
	passCreateInfo.dependencyCount = 0;
	passCreateInfo.pDependencies = nullptr;
	HRI_VK_CHECK(vkCreateRenderPass(m_pCtx->device, &passCreateInfo, nullptr, &m_renderPass));

	// Set up shaders after render pass creation
	size_t presentVertShaderSize = sizeof(uint32_t) * HRI_SIZEOF_ARRAY(gGlslPresentVertShader);
	size_t presentFragShaderSize = sizeof(uint32_t) * HRI_SIZEOF_ARRAY(gGlslPresentFragShader);
	shaderDB->registerShader(HRI_SHADER_DB_BUILTIN_NAME("PresentVert"), Shader::init(m_pCtx, gGlslPresentVertShader, presentVertShaderSize, VK_SHADER_STAGE_VERTEX_BIT));
	shaderDB->registerShader(HRI_SHADER_DB_BUILTIN_NAME("PresentFrag"),	Shader::init(m_pCtx, gGlslPresentFragShader, presentFragShaderSize, VK_SHADER_STAGE_FRAGMENT_BIT));

	m_pBuiltinPSO = shaderDB->createPipeline(
		HRI_SHADER_DB_BUILTIN_NAME("Present"),
		{ HRI_SHADER_DB_BUILTIN_NAME("PresentVert"), HRI_SHADER_DB_BUILTIN_NAME("PresentFrag") },
		builtinPipelineBuilder()
	);

	createResources();
}

BuiltinRenderPass::~BuiltinRenderPass()
{
	for (auto const& framebuffer : m_framebuffers)
	{
		vkDestroyFramebuffer(m_pCtx->device, framebuffer, nullptr);
	}

	vkDestroyRenderPass(m_pCtx->device, m_renderPass, nullptr);
	m_pCtx->swapchain.destroy_image_views(m_swapViews);
}

void BuiltinRenderPass::createResources()
{
	VkExtent2D swapExtent = m_pCtx->swapchain.extent;
	m_swapViews = m_pCtx->swapchain.get_image_views().value();
	m_framebuffers.reserve(m_swapViews.size());
	for (auto const& view : m_swapViews)
	{
		VkFramebufferCreateInfo fbCreateInfo = VkFramebufferCreateInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
		fbCreateInfo.flags = 0;
		fbCreateInfo.renderPass = m_renderPass;
		fbCreateInfo.attachmentCount = 1;
		fbCreateInfo.pAttachments = &view;
		fbCreateInfo.width = swapExtent.width;
		fbCreateInfo.height = swapExtent.height;
		fbCreateInfo.layers = 1;

		VkFramebuffer framebuffer = VK_NULL_HANDLE;
		vkCreateFramebuffer(m_pCtx->device, &fbCreateInfo, nullptr, &framebuffer);
		m_framebuffers.push_back(framebuffer);
	}
}

void BuiltinRenderPass::destroyResources()
{
	for (auto const& framebuffer : m_framebuffers)
	{
		vkDestroyFramebuffer(m_pCtx->device, framebuffer, nullptr);
	}
	m_pCtx->swapchain.destroy_image_views(m_swapViews);
	m_framebuffers.clear();
}

void BuiltinRenderPass::execute(VkCommandBuffer commandBuffer, uint32_t activeSwapImage) const
{
	assert(activeSwapImage < m_framebuffers.size());
	assert(m_pBuiltinPSO != nullptr);

	VkExtent2D swapExtent = m_pCtx->swapchain.extent;
	VkFramebuffer activeFramebuffer = m_framebuffers[activeSwapImage];

	VkViewport viewport = VkViewport{
		0.0f, 0.0f,
		static_cast<float>(swapExtent.width), static_cast<float>(swapExtent.height),
		0.0f, 1.0f
	};

	VkRect2D scissor = VkRect2D{
		VkOffset2D{ 0, 0 },
		swapExtent
	};

	VkClearValue swapClearValue = VkClearValue{};	// Just clear swap to black
	VkRenderPassBeginInfo passBeginInfo = VkRenderPassBeginInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	passBeginInfo.renderPass = m_renderPass;
	passBeginInfo.framebuffer = activeFramebuffer;
	passBeginInfo.renderArea = scissor;
	passBeginInfo.clearValueCount = 1;
	passBeginInfo.pClearValues = &swapClearValue;
	vkCmdBeginRenderPass(commandBuffer, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	vkCmdBindPipeline(commandBuffer, m_pBuiltinPSO->bindPoint, m_pBuiltinPSO->pipeline);
	vkCmdDraw(commandBuffer, 3, 1, 0, 0);

	vkCmdEndRenderPass(commandBuffer);
}

GraphicsPipelineBuilder BuiltinRenderPass::builtinPipelineBuilder() const
{
	static const std::vector<VkPipelineColorBlendAttachmentState> blendAttachments = {
		VkPipelineColorBlendAttachmentState{
			VK_FALSE,
			VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO,
			VK_BLEND_OP_ADD,
			VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO,
			VK_BLEND_OP_ADD,
			VK_COLOR_COMPONENT_R_BIT
			| VK_COLOR_COMPONENT_G_BIT
			| VK_COLOR_COMPONENT_B_BIT
			| VK_COLOR_COMPONENT_A_BIT
		}
	};

	GraphicsPipelineBuilder builtinBuilder = GraphicsPipelineBuilder{};
	builtinBuilder.vertexInputBindings = {};
	builtinBuilder.vertexInputAttributes = {};
	builtinBuilder.inputAssemblyState = GraphicsPipelineBuilder::initInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE);
	builtinBuilder.viewport = GraphicsPipelineBuilder::initDefaultViewport(1.0f, 1.0f);
	builtinBuilder.scissor = GraphicsPipelineBuilder::initDefaultScissor(1, 1);
	builtinBuilder.rasterizationState = GraphicsPipelineBuilder::initRasterizationState(VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	builtinBuilder.multisampleState = GraphicsPipelineBuilder::initMultisampleState(VK_SAMPLE_COUNT_1_BIT);
	builtinBuilder.depthStencilState = GraphicsPipelineBuilder::initDepthStencilState(VK_FALSE, VK_FALSE);
	builtinBuilder.colorBlendState = GraphicsPipelineBuilder::initColorBlendState(blendAttachments);
	builtinBuilder.dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	builtinBuilder.renderPass = m_renderPass;
	builtinBuilder.subpass = 0;

	return builtinBuilder;
}
