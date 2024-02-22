#include "subsystems.h"

#include <hybrid_renderer.h>
#include <imgui_impl_vulkan.h>

GBufferLayoutSubsystem::GBufferLayoutSubsystem(
	hri::RenderContext* ctx,
	hri::DescriptorSetAllocator* descriptorSetAllocator,
	hri::ShaderDatabase* shaderDB,
	VkRenderPass renderPass,
	hri::DescriptorSetLayout& sceneDataSetLayout,
	hri::BatchedSceneData& batchedScene
)
	:
	hri::IRenderSubsystem(ctx, descriptorSetAllocator),
	m_batchedScene(batchedScene)
{
	m_layout = hri::PipelineLayoutBuilder(ctx)
		.addDescriptorSetLayout(sceneDataSetLayout)
		.build();

	std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
		VkVertexInputBindingDescription{ 0, sizeof(hri::Vertex), VK_VERTEX_INPUT_RATE_VERTEX }
	};

	std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
		VkVertexInputAttributeDescription{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(hri::Vertex, position) },
		VkVertexInputAttributeDescription{ 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(hri::Vertex, normal) },
		VkVertexInputAttributeDescription{ 2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(hri::Vertex, tangent) },
		VkVertexInputAttributeDescription{ 3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(hri::Vertex, textureCoord) },
	};

	std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments = {
		VkPipelineColorBlendAttachmentState{
			VK_FALSE,
			VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
			VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
			VK_COLOR_COMPONENT_R_BIT
			| VK_COLOR_COMPONENT_G_BIT
			| VK_COLOR_COMPONENT_B_BIT
			| VK_COLOR_COMPONENT_A_BIT
		},
		VkPipelineColorBlendAttachmentState{
			VK_FALSE,
			VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
			VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
			VK_COLOR_COMPONENT_R_BIT
			| VK_COLOR_COMPONENT_G_BIT
			| VK_COLOR_COMPONENT_B_BIT
			| VK_COLOR_COMPONENT_A_BIT
		},
		VkPipelineColorBlendAttachmentState{
			VK_FALSE,
			VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
			VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
			VK_COLOR_COMPONENT_R_BIT
			| VK_COLOR_COMPONENT_G_BIT
			| VK_COLOR_COMPONENT_B_BIT
			| VK_COLOR_COMPONENT_A_BIT
		}
	};

	hri::GraphicsPipelineBuilder gbufferLayoutPipelineConfig = hri::GraphicsPipelineBuilder{};
	gbufferLayoutPipelineConfig.vertexInputBindings = vertexInputBindings;
	gbufferLayoutPipelineConfig.vertexInputAttributes = vertexInputAttributes;
	gbufferLayoutPipelineConfig.inputAssemblyState = hri::GraphicsPipelineBuilder::initInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE);
	gbufferLayoutPipelineConfig.viewport = hri::GraphicsPipelineBuilder::initDefaultViewport(1.0f, 1.0f);
	gbufferLayoutPipelineConfig.scissor = hri::GraphicsPipelineBuilder::initDefaultScissor(1, 1);
	gbufferLayoutPipelineConfig.rasterizationState = hri::GraphicsPipelineBuilder::initRasterizationState(VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	gbufferLayoutPipelineConfig.multisampleState = hri::GraphicsPipelineBuilder::initMultisampleState(VK_SAMPLE_COUNT_1_BIT);
	gbufferLayoutPipelineConfig.depthStencilState = hri::GraphicsPipelineBuilder::initDepthStencilState(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS);
	gbufferLayoutPipelineConfig.colorBlendState = hri::GraphicsPipelineBuilder::initColorBlendState(colorBlendAttachments);
	gbufferLayoutPipelineConfig.dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	gbufferLayoutPipelineConfig.layout = m_layout;
	gbufferLayoutPipelineConfig.renderPass = renderPass;
	gbufferLayoutPipelineConfig.subpass = 0;

	m_pPSO = shaderDB->createPipeline(
		"GBufferLayoutPipeline",
		{ "StaticVert", "GBufferLayoutFrag" },
		gbufferLayoutPipelineConfig
	);
}

void GBufferLayoutSubsystem::record(hri::ActiveFrame& frame) const
{
	assert(m_pPSO != nullptr);

	VkExtent2D swapExtent = m_pCtx->swapchain.extent;

	VkViewport viewport = VkViewport{
		0.0f, 0.0f,
		static_cast<float>(swapExtent.width), static_cast<float>(swapExtent.height),
		hri::DefaultViewportMinDepth, hri::DefaultViewportMaxDepth
	};

	VkRect2D scissor = VkRect2D
	{
		VkOffset2D{ 0, 0 },
		swapExtent
	};

	vkCmdSetViewport(frame.commandBuffer, 0, 1, &viewport);
	vkCmdSetScissor(frame.commandBuffer, 0, 1, &scissor);

	vkCmdBindPipeline(frame.commandBuffer, m_pPSO->bindPoint, m_pPSO->pipeline);

	for (auto const& [ materialIdx, meshes ] : m_batchedScene.batchedMeshes)
	{
		for (auto const& mesh : meshes)
		{
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(frame.commandBuffer, 0, 1, &mesh.vertexBuffer.buffer, offsets);
			vkCmdBindIndexBuffer(frame.commandBuffer, mesh.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(frame.commandBuffer, mesh.indexCount, 1, 0, 0, 0);
		}
	}
}

void GBufferLayoutSubsystem::updatedBatchedScene(hri::BatchedSceneData& batchedScene)
{
	m_batchedScene = batchedScene;
}

UISubsystem::UISubsystem(
	hri::RenderContext* ctx,
	hri::DescriptorSetAllocator* descriptorSetAllocator,
	hri::ShaderDatabase* shaderDB,
	VkRenderPass renderPass
)
	:
	hri::IRenderSubsystem(ctx, descriptorSetAllocator)
{
	ImGui_ImplVulkan_InitInfo initInfo = {};
	initInfo.Instance = m_pCtx->instance;
	initInfo.PhysicalDevice = m_pCtx->gpu;
	initInfo.Device = m_pCtx->device;
	initInfo.QueueFamily = m_pCtx->queues.graphicsQueue.family;
	initInfo.Queue = m_pCtx->queues.graphicsQueue.handle;
	initInfo.PipelineCache = VK_NULL_HANDLE;
	initInfo.DescriptorPool = m_pDescriptorSetAllocator->fixedPool();
	initInfo.Subpass = 0;
	initInfo.MinImageCount = m_pCtx->swapchain.requested_min_image_count;
	initInfo.ImageCount = m_pCtx->swapchain.image_count;
	initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	initInfo.RenderPass = renderPass;
	ImGui_ImplVulkan_Init(&initInfo);
}

UISubsystem::~UISubsystem()
{
	ImGui_ImplVulkan_Shutdown();
}

void UISubsystem::record(hri::ActiveFrame& frame) const
{
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), frame.commandBuffer);
}

PresentationSubsystem::PresentationSubsystem(
	hri::RenderContext* ctx,
	hri::DescriptorSetAllocator* descriptorSetAllocator,
	hri::ShaderDatabase* shaderDB,
	VkRenderPass renderPass,
	hri::DescriptorSetLayout& presentInputSetLayout
)
	:
	hri::IRenderSubsystem(ctx, descriptorSetAllocator)
{
	m_layout = hri::PipelineLayoutBuilder(ctx)
		.addDescriptorSetLayout(presentInputSetLayout)
		.build();

	std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments = {
		VkPipelineColorBlendAttachmentState{
			VK_FALSE,
			VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
			VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
			VK_COLOR_COMPONENT_R_BIT
			| VK_COLOR_COMPONENT_G_BIT
			| VK_COLOR_COMPONENT_B_BIT
			| VK_COLOR_COMPONENT_A_BIT
		}
	};

	hri::GraphicsPipelineBuilder presentPipelineConfig = hri::GraphicsPipelineBuilder{};
	presentPipelineConfig.vertexInputBindings = {};
	presentPipelineConfig.vertexInputAttributes = {};
	presentPipelineConfig.inputAssemblyState = hri::GraphicsPipelineBuilder::initInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE);
	presentPipelineConfig.viewport = hri::GraphicsPipelineBuilder::initDefaultViewport(1.0f, 1.0f);
	presentPipelineConfig.scissor = hri::GraphicsPipelineBuilder::initDefaultScissor(1, 1);
	presentPipelineConfig.rasterizationState = hri::GraphicsPipelineBuilder::initRasterizationState(VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	presentPipelineConfig.multisampleState = hri::GraphicsPipelineBuilder::initMultisampleState(VK_SAMPLE_COUNT_1_BIT);
	presentPipelineConfig.depthStencilState = hri::GraphicsPipelineBuilder::initDepthStencilState(VK_FALSE, VK_FALSE);
	presentPipelineConfig.colorBlendState = hri::GraphicsPipelineBuilder::initColorBlendState(colorBlendAttachments);
	presentPipelineConfig.dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	presentPipelineConfig.layout = m_layout;
	presentPipelineConfig.renderPass = renderPass;
	presentPipelineConfig.subpass = 0;

	m_pPSO = shaderDB->createPipeline(
		"PresentPipeline",
		{ "PresentVert", "PresentFrag" },
		presentPipelineConfig
	);
}

void PresentationSubsystem::record(hri::ActiveFrame& frame) const
{
	assert(m_pPSO != nullptr);

	VkExtent2D swapExtent = m_pCtx->swapchain.extent;

	VkViewport viewport = VkViewport{
		0.0f, 0.0f,
		static_cast<float>(swapExtent.width), static_cast<float>(swapExtent.height),
		hri::DefaultViewportMinDepth, hri::DefaultViewportMaxDepth
	};

	VkRect2D scissor = VkRect2D
	{
		VkOffset2D{ 0, 0 },
		swapExtent
	};

	vkCmdSetViewport(frame.commandBuffer, 0, 1, &viewport);
	vkCmdSetScissor(frame.commandBuffer, 0, 1, &scissor);

	vkCmdBindPipeline(frame.commandBuffer, m_pPSO->bindPoint, m_pPSO->pipeline);
	vkCmdDraw(frame.commandBuffer, 3, 1, 0, 0);
}
