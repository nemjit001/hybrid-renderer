#include "subsystems.h"

#include <hybrid_renderer.h>
#include <imgui_impl_vulkan.h>
#include <vector>

GBufferLayoutSubsystem::GBufferLayoutSubsystem(
	hri::RenderContext& ctx,
	hri::ShaderDatabase& shaderDB,
	VkRenderPass renderPass,
	VkDescriptorSetLayout sceneDataSetLayout
)
	:
	hri::IRenderSubsystem(ctx)
{
	m_layout = hri::PipelineLayoutBuilder(ctx)
		.addPushConstant(
			sizeof(InstancePushConstant),
			VK_SHADER_STAGE_VERTEX_BIT
			| VK_SHADER_STAGE_FRAGMENT_BIT
		)
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

	m_pPSO = shaderDB.createPipeline(
		"GBufferLayoutPipeline",
		{ "StaticVert", "GBufferLayoutFrag" },
		gbufferLayoutPipelineConfig
	);
}

void GBufferLayoutSubsystem::record(hri::ActiveFrame& frame) const
{
	assert(m_pPSO != nullptr);
	assert(m_currentFrameInfo.sceneDataSetHandle != VK_NULL_HANDLE);
	m_debug.cmdBeginLabel(frame.commandBuffer, "GBuffer Layout Pass");

	VkExtent2D swapExtent = m_ctx.swapchain.extent;

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

	vkCmdBindDescriptorSets(
		frame.commandBuffer,
		m_pPSO->bindPoint,
		m_layout,
		0, 1, &m_currentFrameInfo.sceneDataSetHandle,
		0, nullptr
	);

	vkCmdBindPipeline(frame.commandBuffer, m_pPSO->bindPoint, m_pPSO->pipeline);

	for (auto const& instance : m_currentFrameInfo.pSceneGraph->getRenderInstanceList())
	{
		InstancePushConstant instancePC = InstancePushConstant{
			instance.instanceId,
			instance.modelMatrix,
			hri::Float3x3(1.0f),
		};

		vkCmdPushConstants(
			frame.commandBuffer,
			m_layout,
			VK_SHADER_STAGE_VERTEX_BIT
			| VK_SHADER_STAGE_FRAGMENT_BIT,
			0, sizeof(InstancePushConstant),
			&instancePC
		);

		const hri::Mesh& mesh = m_currentFrameInfo.pSceneGraph->meshes[instance.instanceId];

		VkDeviceSize vertexOffsets[] = { 0 };
		vkCmdBindVertexBuffers(frame.commandBuffer, 0, 1, &mesh.vertexBuffer.buffer, vertexOffsets);
		vkCmdBindIndexBuffer(frame.commandBuffer, mesh.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(frame.commandBuffer, mesh.indexCount, 1, 0, 0, 0);
	}

	m_debug.cmdEndLabel(frame.commandBuffer);
}

IRayTracingSubSystem::IRayTracingSubSystem(raytracing::RayTracingContext& ctx)
	:
	m_rtCtx(ctx),
	hri::IRenderSubsystem(ctx.renderContext)
{
	//
}

HybridRayTracingSubsystem::HybridRayTracingSubsystem(
	raytracing::RayTracingContext& ctx,
	hri::ShaderDatabase& shaderDB,
	VkDescriptorSetLayout sceneDataSetLayout,
	VkDescriptorSetLayout rtSetLayout
)
	:
	IRayTracingSubSystem(ctx)
{
	m_layout = hri::PipelineLayoutBuilder(ctx.renderContext)
		.addDescriptorSetLayout(sceneDataSetLayout)
		.addDescriptorSetLayout(rtSetLayout)
		.build();

	// Soft shadows
	hri::Shader* pSSRayGen = shaderDB.getShader("SSRayGen");
	hri::Shader* pSSMiss = shaderDB.getShader("SSMiss");

	// Direct Illumination
	hri::Shader* pDIRayGen = shaderDB.getShader("DIRayGen");
	hri::Shader* pDIMiss = shaderDB.getShader("DIMiss");
	hri::Shader* pDICHit = shaderDB.getShader("DICHit");

	raytracing::RayTracingPipelineBuilder rtPipelineBuilder = raytracing::RayTracingPipelineBuilder(ctx)
		.addShaderStage(pSSRayGen->stage, pSSRayGen->module)
		.addShaderStage(pSSMiss->stage, pSSMiss->module)
		.addShaderStage(pDIRayGen->stage, pDIRayGen->module)
		.addShaderStage(pDIMiss->stage, pDIMiss->module)
		.addShaderStage(pDICHit->stage, pDICHit->module)
		// SS shader groups
		.addRayTracingShaderGroup(VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 0)
		.addRayTracingShaderGroup(VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 1)
		// DI shader groups
		.addRayTracingShaderGroup(VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 2)
		.addRayTracingShaderGroup(VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 3)
		.addRayTracingShaderGroup(VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR, VK_SHADER_UNUSED_KHR, 4)
		.setMaxRecursionDepth()
		.setLayout(m_layout);

	VkPipeline raytracingPipeline = rtPipelineBuilder.build(shaderDB.pipelineCache());
	m_pPSO = shaderDB.registerPipeline(
		"HybridRenderingRTPipeline",
		VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
		raytracingPipeline);

	// Init the SBT
	m_SBT = std::unique_ptr<raytracing::ShaderBindingTable>(new raytracing::ShaderBindingTable(
		m_rtCtx,
		raytracingPipeline,
		rtPipelineBuilder
	));
}

void HybridRayTracingSubsystem::record(hri::ActiveFrame& frame) const
{
	assert(m_pPSO != nullptr);
	m_debug.cmdBeginLabel(frame.commandBuffer, "Hybrid Rendering Raytracing Pass");

	VkExtent2D swapExtent = m_ctx.swapchain.extent;

	VkDescriptorSet descriptorSets[] = {
		m_currentFrameInfo.sceneDataSetHandle,
		m_currentFrameInfo.raytracingSetHandle,
	};

	vkCmdBindDescriptorSets(
		frame.commandBuffer,
		m_pPSO->bindPoint,
		m_layout,
		0, 2, descriptorSets,
		0, nullptr
	);

	vkCmdBindPipeline(frame.commandBuffer, m_pPSO->bindPoint, m_pPSO->pipeline);

	VkStridedDeviceAddressRegionKHR SSRayGenRegion = m_SBT->getRegion(raytracing::ShaderBindingTable::SBTShaderGroup::SGRayGen, 0);
	VkStridedDeviceAddressRegionKHR DIRayGenRegion = m_SBT->getRegion(raytracing::ShaderBindingTable::SBTShaderGroup::SGRayGen, 1);
	VkStridedDeviceAddressRegionKHR missRegion = m_SBT->getRegion(raytracing::ShaderBindingTable::SBTShaderGroup::SGMiss);
	VkStridedDeviceAddressRegionKHR hitRegion = m_SBT->getRegion(raytracing::ShaderBindingTable::SBTShaderGroup::SGHit);
	VkStridedDeviceAddressRegionKHR callRegion = m_SBT->getRegion(raytracing::ShaderBindingTable::SBTShaderGroup::SGCall);

	// Soft Shadow subpass
	m_rtCtx.rayTracingDispatch.vkCmdTraceRays(
		frame.commandBuffer,
		&SSRayGenRegion,
		&missRegion,
		&hitRegion,
		&callRegion,
		swapExtent.width,
		swapExtent.height,
		1
	);

	// Direct Illumination subpass
	m_rtCtx.rayTracingDispatch.vkCmdTraceRays(
		frame.commandBuffer,
		&DIRayGenRegion,
		&missRegion,
		&hitRegion,
		&callRegion,
		swapExtent.width,
		swapExtent.height,
		1
	);

	m_debug.cmdEndLabel(frame.commandBuffer);
}

UISubsystem::UISubsystem(
	hri::RenderContext& ctx,
	VkRenderPass renderPass,
	VkDescriptorPool uiPool
)
	:
	hri::IRenderSubsystem(ctx)
{
	ImGui_ImplVulkan_InitInfo initInfo = {};
	initInfo.Instance = m_ctx.instance;
	initInfo.PhysicalDevice = m_ctx.gpu;
	initInfo.Device = m_ctx.device;
	initInfo.QueueFamily = m_ctx.queues.graphicsQueue.family;
	initInfo.Queue = m_ctx.queues.graphicsQueue.handle;
	initInfo.PipelineCache = VK_NULL_HANDLE;
	initInfo.DescriptorPool = uiPool;
	initInfo.Subpass = 0;
	initInfo.MinImageCount = m_ctx.swapchain.requested_min_image_count;
	initInfo.ImageCount = m_ctx.swapchain.image_count;
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
	m_debug.cmdBeginLabel(frame.commandBuffer, "UI Pass");
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), frame.commandBuffer);
	m_debug.cmdEndLabel(frame.commandBuffer);
}

PresentationSubsystem::PresentationSubsystem(
	hri::RenderContext& ctx,
	hri::ShaderDatabase& shaderDB,
	VkRenderPass renderPass,
	VkDescriptorSetLayout presentInputSetLayout
)
	:
	hri::IRenderSubsystem(ctx)
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

	m_pPSO = shaderDB.createPipeline(
		"PresentPipeline",
		{ "PresentVert", "PresentFrag" },
		presentPipelineConfig
	);
}

void PresentationSubsystem::record(hri::ActiveFrame& frame) const
{
	assert(m_pPSO != nullptr);
	assert(m_currentFrameInfo.presentInputSetHandle != VK_NULL_HANDLE);
	m_debug.cmdBeginLabel(frame.commandBuffer, "Present Pass");

	VkExtent2D swapExtent = m_ctx.swapchain.extent;

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

	vkCmdBindDescriptorSets(
		frame.commandBuffer,
		m_pPSO->bindPoint,
		m_layout,
		0, 1, &m_currentFrameInfo.presentInputSetHandle,
		0, nullptr
	);

	vkCmdBindPipeline(frame.commandBuffer, m_pPSO->bindPoint, m_pPSO->pipeline);
	vkCmdDraw(frame.commandBuffer, 3, 1, 0, 0);

	m_debug.cmdEndLabel(frame.commandBuffer);
}
