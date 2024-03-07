#include "subsystems.h"

#include <hybrid_renderer.h>
#include <imgui_impl_vulkan.h>
#include <vector>

/// @brief The Instance Push Constant is used to push instance & transformation data for renderable objects to shaders.
struct InstancePushConstant
{
	HRI_ALIGNAS(4)  uint32_t instanceId;
	HRI_ALIGNAS(16) hri::Float4x4 model = hri::Float4x4(1.0f);
	HRI_ALIGNAS(16) hri::Float3x3 normal = hri::Float3x3(1.0f);
};

/// @brief The RT Frame Info PC contains per frame data needed for ray tracing.
struct RTFrameInfoPushConstant
{
	HRI_ALIGNAS(4) uint32_t frameIdx;
};

GBufferLayoutSubsystem::GBufferLayoutSubsystem(
	hri::RenderContext& ctx,
	hri::ShaderDatabase& shaderDB,
	VkRenderPass renderPass,
	VkDescriptorSetLayout sceneDataSetLayout
)
	:
	hri::IRenderSubsystem<GBufferLayoutFrameInfo>(ctx)
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

void GBufferLayoutSubsystem::record(hri::ActiveFrame& frame, GBufferLayoutFrameInfo& frameInfo) const
{
	assert(m_pPSO != nullptr);
	assert(frameInfo.sceneDataSetHandle != VK_NULL_HANDLE);
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
		0, 1, &frameInfo.sceneDataSetHandle,
		0, nullptr
	);

	vkCmdBindPipeline(frame.commandBuffer, m_pPSO->bindPoint, m_pPSO->pipeline);

	for (auto const& instance : frameInfo.pSceneGraph->getRenderInstanceList())
	{
		InstancePushConstant instancePC = InstancePushConstant{
			instance.instanceIdLOD0,
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

		const hri::Mesh& mesh = frameInfo.pSceneGraph->meshes[instance.instanceIdLOD0];

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
	hri::IRenderSubsystem<RayTracingFrameInfo>(ctx.renderContext)
{
	//
}

void IRayTracingSubSystem::transitionGbufferResources(hri::ActiveFrame& frame, RayTracingFrameInfo& frameInfo) const
{
	VkImageMemoryBarrier2 gbufferSampleBarrier = VkImageMemoryBarrier2{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
	gbufferSampleBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	gbufferSampleBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	gbufferSampleBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	gbufferSampleBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
	gbufferSampleBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	gbufferSampleBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	gbufferSampleBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	gbufferSampleBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

	gbufferSampleBarrier.image = frameInfo.gbufferAlbedo->image;
	gbufferSampleBarrier.subresourceRange = hri::ImageResource::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);
	frame.pipelineBarrier({ gbufferSampleBarrier });

	gbufferSampleBarrier.image = frameInfo.gbufferEmission->image;
	gbufferSampleBarrier.subresourceRange = hri::ImageResource::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);
	frame.pipelineBarrier({ gbufferSampleBarrier });

	gbufferSampleBarrier.image = frameInfo.gbufferSpecular->image;
	gbufferSampleBarrier.subresourceRange = hri::ImageResource::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);
	frame.pipelineBarrier({ gbufferSampleBarrier });

	gbufferSampleBarrier.image = frameInfo.gbufferTransmittance->image;
	gbufferSampleBarrier.subresourceRange = hri::ImageResource::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);
	frame.pipelineBarrier({ gbufferSampleBarrier });

	gbufferSampleBarrier.image = frameInfo.gbufferNormal->image;
	gbufferSampleBarrier.subresourceRange = hri::ImageResource::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);
	frame.pipelineBarrier({ gbufferSampleBarrier });

	gbufferSampleBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	gbufferSampleBarrier.image = frameInfo.gbufferDepth->image;
	gbufferSampleBarrier.subresourceRange = hri::ImageResource::SubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1);
	frame.pipelineBarrier({ gbufferSampleBarrier });
}

GlobalIlluminationSubsystem::GlobalIlluminationSubsystem(
	raytracing::RayTracingContext& ctx,
	hri::ShaderDatabase& shaderDB,
	VkDescriptorSetLayout sceneDataSetLayout,
	VkDescriptorSetLayout rtSetLayout
)
	:
	IRayTracingSubSystem(ctx)
{
	m_layout = hri::PipelineLayoutBuilder(ctx.renderContext)
		.addPushConstant(sizeof(RTFrameInfoPushConstant), VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.addDescriptorSetLayout(sceneDataSetLayout)
		.addDescriptorSetLayout(rtSetLayout)
		.build();

	// Direct Illumination
	hri::Shader* pGIRayGen = shaderDB.getShader("GIRayGen");
	hri::Shader* pGIMiss = shaderDB.getShader("GIMiss");
	hri::Shader* pGICHit = shaderDB.getShader("GICHit");

	raytracing::RayTracingPipelineBuilder rtPipelineBuilder = raytracing::RayTracingPipelineBuilder(ctx)
		.addShaderStage(pGIRayGen->stage, pGIRayGen->module)
		.addShaderStage(pGIMiss->stage, pGIMiss->module)
		.addShaderStage(pGICHit->stage, pGICHit->module)
		.addRayTracingShaderGroup(VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 0)
		.addRayTracingShaderGroup(VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 1)
		.addRayTracingShaderGroup(VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR, VK_SHADER_UNUSED_KHR, 2)
		.setMaxRecursionDepth()
		.setLayout(m_layout);

	VkPipeline raytracingPipeline = rtPipelineBuilder.build(shaderDB.pipelineCache());
	m_pPSO = shaderDB.registerPipeline(
		"GIRTPipeline",
		VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
		raytracingPipeline);

	// Init the SBT
	m_SBT = std::unique_ptr<raytracing::ShaderBindingTable>(new raytracing::ShaderBindingTable(
		m_rtCtx,
		raytracingPipeline,
		rtPipelineBuilder
	));
}

void GlobalIlluminationSubsystem::record(hri::ActiveFrame& frame, RayTracingFrameInfo& frameInfo) const
{
	assert(m_pPSO != nullptr);
	m_debug.cmdBeginLabel(frame.commandBuffer, "GI Raytracing Pass");
	VkExtent2D swapExtent = m_ctx.swapchain.extent;

	// pass resource barriers
	{
		VkImageMemoryBarrier2 storageImageBarrier = VkImageMemoryBarrier2{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
		storageImageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		storageImageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
		storageImageBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		storageImageBarrier.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT_KHR;
		storageImageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		storageImageBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		storageImageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		storageImageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		storageImageBarrier.image = frameInfo.globalIlluminationOut->image;
		storageImageBarrier.subresourceRange = hri::ImageResource::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);

		frame.pipelineBarrier({ storageImageBarrier });
	}

	VkDescriptorSet descriptorSets[] = {
		frameInfo.sceneDataSetHandle,
		frameInfo.raytracingSetHandle,
	};

	vkCmdBindDescriptorSets(
		frame.commandBuffer,
		m_pPSO->bindPoint,
		m_layout,
		0, 2, descriptorSets,
		0, nullptr
	);

	RTFrameInfoPushConstant frameInfoPC = RTFrameInfoPushConstant{
		frameInfo.frameCounter,
	};

	vkCmdPushConstants(
		frame.commandBuffer,
		m_layout,
		VK_SHADER_STAGE_RAYGEN_BIT_KHR,
		0, sizeof(RTFrameInfoPushConstant),
		&frameInfoPC
	);

	vkCmdBindPipeline(frame.commandBuffer, m_pPSO->bindPoint, m_pPSO->pipeline);

	VkStridedDeviceAddressRegionKHR RayGenRegion = m_SBT->getRegion(raytracing::ShaderBindingTable::SBTShaderGroup::SGRayGen, 0);
	VkStridedDeviceAddressRegionKHR missRegion = m_SBT->getRegion(raytracing::ShaderBindingTable::SBTShaderGroup::SGMiss);
	VkStridedDeviceAddressRegionKHR hitRegion = m_SBT->getRegion(raytracing::ShaderBindingTable::SBTShaderGroup::SGHit);
	VkStridedDeviceAddressRegionKHR callRegion = m_SBT->getRegion(raytracing::ShaderBindingTable::SBTShaderGroup::SGCall);

	// Soft Shadow subpass
	m_rtCtx.rayTracingDispatch.vkCmdTraceRays(
		frame.commandBuffer,
		&RayGenRegion,
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
	hri::IRenderSubsystem<UIFrameInfo>(ctx)
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

void UISubsystem::record(hri::ActiveFrame& frame, UIFrameInfo& frameInfo) const
{
	m_debug.cmdBeginLabel(frame.commandBuffer, "UI Pass");
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), frame.commandBuffer);
	m_debug.cmdEndLabel(frame.commandBuffer);
}

DeferredShadingSubsystem::DeferredShadingSubsystem(
	hri::RenderContext& ctx,
	hri::ShaderDatabase& shaderDB,
	VkRenderPass renderPass,
	VkDescriptorSetLayout composeSetLayout
)
	:
	hri::IRenderSubsystem<DeferredShadingFrameInfo>(ctx)
{
	m_layout = hri::PipelineLayoutBuilder(ctx)
		.addDescriptorSetLayout(composeSetLayout)
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

	hri::GraphicsPipelineBuilder composePipelineConfig = hri::GraphicsPipelineBuilder{};
	composePipelineConfig.vertexInputBindings = {};
	composePipelineConfig.vertexInputAttributes = {};
	composePipelineConfig.inputAssemblyState = hri::GraphicsPipelineBuilder::initInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE);
	composePipelineConfig.viewport = hri::GraphicsPipelineBuilder::initDefaultViewport(1.0f, 1.0f);
	composePipelineConfig.scissor = hri::GraphicsPipelineBuilder::initDefaultScissor(1, 1);
	composePipelineConfig.rasterizationState = hri::GraphicsPipelineBuilder::initRasterizationState(VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	composePipelineConfig.multisampleState = hri::GraphicsPipelineBuilder::initMultisampleState(VK_SAMPLE_COUNT_1_BIT);
	composePipelineConfig.depthStencilState = hri::GraphicsPipelineBuilder::initDepthStencilState(VK_FALSE, VK_FALSE);
	composePipelineConfig.colorBlendState = hri::GraphicsPipelineBuilder::initColorBlendState(colorBlendAttachments);
	composePipelineConfig.dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	composePipelineConfig.layout = m_layout;
	composePipelineConfig.renderPass = renderPass;
	composePipelineConfig.subpass = 0;

	m_pPSO = shaderDB.createPipeline(
		"DeferredShadingPipeline",
		{ "PresentVert", "DeferredShadingFrag" },
		composePipelineConfig
	);
}

void DeferredShadingSubsystem::record(hri::ActiveFrame& frame, DeferredShadingFrameInfo& frameInfo) const
{
	assert(m_pPSO != nullptr);
	assert(frameInfo.deferedShadingSetHandle != VK_NULL_HANDLE);
	m_debug.cmdBeginLabel(frame.commandBuffer, "Deferred Pass");
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
		0, 1, &frameInfo.deferedShadingSetHandle,
		0, nullptr
	);

	vkCmdBindPipeline(frame.commandBuffer, m_pPSO->bindPoint, m_pPSO->pipeline);
	vkCmdDraw(frame.commandBuffer, 3, 1, 0, 0);

	m_debug.cmdEndLabel(frame.commandBuffer);
}

PresentationSubsystem::PresentationSubsystem(
	hri::RenderContext& ctx,
	hri::ShaderDatabase& shaderDB,
	VkRenderPass renderPass,
	VkDescriptorSetLayout presentInputSetLayout
)
	:
	hri::IRenderSubsystem<PresentFrameInfo>(ctx)
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

void PresentationSubsystem::record(hri::ActiveFrame& frame, PresentFrameInfo& frameInfo) const
{
	assert(m_pPSO != nullptr);
	assert(frameInfo.presentInputSetHandle != VK_NULL_HANDLE);
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
		0, 1, &frameInfo.presentInputSetHandle,
		0, nullptr
	);

	vkCmdBindPipeline(frame.commandBuffer, m_pPSO->bindPoint, m_pPSO->pipeline);
	vkCmdDraw(frame.commandBuffer, 3, 1, 0, 0);

	m_debug.cmdEndLabel(frame.commandBuffer);
}
