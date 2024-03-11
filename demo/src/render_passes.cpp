#include "render_passes.h"

#include <hybrid_renderer.h>
#include <imgui_impl_vulkan.h>
#include <memory>

#include "detail/raytracing.h"
#include "scene.h"

// --- IRenderPass ---

IRenderPass::IRenderPass(hri::RenderContext& ctx)
	:
	context(ctx)
{
	//
}

void IRenderPass::prepareFrame(CommonResources& resources)
{
	// Don't do anything by default
}

// --- Path Tracing Pass ---

PathTracingPass::PathTracingPass(raytracing::RayTracingContext& ctx, hri::ShaderDatabase& shaderDB, hri::DescriptorSetAllocator& descriptorAllocator)
	:
	IRenderPass(ctx.renderContext),
	rtContext(ctx)
{
	recreateResources(context.swapchain.extent);

	hri::DescriptorSetLayoutBuilder sceneDescriptorSetLayoutBuilder(context);
	sceneDescriptorSetLayoutBuilder
		.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
		.addBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);

	hri::DescriptorSetLayoutBuilder rtDescriptorSetLayoutBuilder(context);
	rtDescriptorSetLayoutBuilder
		.addBinding(0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
		.addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR);

	sceneDescriptorSetLayout = std::make_unique<hri::DescriptorSetLayout>(sceneDescriptorSetLayoutBuilder.build());
	rtDescriptorSetLayout = std::make_unique<hri::DescriptorSetLayout>(rtDescriptorSetLayoutBuilder.build());

	sceneDescriptorSet = std::unique_ptr<hri::DescriptorSetManager>(new hri::DescriptorSetManager(context, descriptorAllocator, *sceneDescriptorSetLayout));
	rtDescriptorSet = std::unique_ptr<hri::DescriptorSetManager>(new hri::DescriptorSetManager(context, descriptorAllocator, *rtDescriptorSetLayout));

	hri::PipelineLayoutBuilder layoutBuilder(context);
	m_layout = layoutBuilder
		.addPushConstant(sizeof(PathTracingPass::PushConstantData), VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.addDescriptorSetLayout(*sceneDescriptorSetLayout)
		.addDescriptorSetLayout(*rtDescriptorSetLayout)
		.build();

	hri::Shader* pRayGen = shaderDB.registerShader("PathTracingRayGen", hri::Shader::loadFile(context, "shaders/pt.rgen.spv", VK_SHADER_STAGE_RAYGEN_BIT_KHR));
	hri::Shader* pMiss = shaderDB.registerShader("PathTracingMiss", hri::Shader::loadFile(context, "shaders/pt.rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR));
	hri::Shader* pCHit = shaderDB.registerShader("PathTracingCHit", hri::Shader::loadFile(context, "shaders/pt.rchit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR));

	raytracing::RayTracingPipelineBuilder pipelineBuilder(rtContext);
	pipelineBuilder
		.addShaderStage(pRayGen->stage, pRayGen->module)
		.addShaderStage(pMiss->stage, pMiss->module)
		.addShaderStage(pCHit->stage, pCHit->module)
		.addRayTracingShaderGroup(VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 0)
		.addRayTracingShaderGroup(VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 1)
		.addRayTracingShaderGroup(VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR, VK_SHADER_UNUSED_KHR, 2)
		.setMaxRecursionDepth()
		.setLayout(m_layout);

	m_pPSO = shaderDB.registerPipeline("PathTracingPipeline", VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineBuilder.build());
	m_SBT = std::unique_ptr<raytracing::ShaderBindingTable>(new raytracing::ShaderBindingTable(rtContext, m_pPSO->pipeline, pipelineBuilder));
}

PathTracingPass::~PathTracingPass()
{
	vkDestroyPipelineLayout(context.device, m_layout, nullptr);
}

void PathTracingPass::prepareFrame(CommonResources& resources)
{
	VkDescriptorBufferInfo cameraInfo = VkDescriptorBufferInfo{};
	cameraInfo.buffer = resources.cameraUBO->buffer;
	cameraInfo.offset = 0;
	cameraInfo.range = resources.cameraUBO->bufferSize;

	VkDescriptorBufferInfo instanceInfo = VkDescriptorBufferInfo{};
	instanceInfo.buffer = resources.instanceDataSSBO->buffer;
	instanceInfo.offset = 0;
	instanceInfo.range = resources.instanceDataSSBO->bufferSize;

	VkDescriptorBufferInfo materialInfo = VkDescriptorBufferInfo{};
	materialInfo.buffer = resources.materialSSBO->buffer;
	materialInfo.offset = 0;
	materialInfo.range = resources.materialSSBO->bufferSize;

	(*sceneDescriptorSet)
		.writeBuffer(0, &cameraInfo)
		.writeBuffer(1, &instanceInfo)
		.writeBuffer(2, &materialInfo)
		.flush();

	VkWriteDescriptorSetAccelerationStructureKHR tlasWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR };
	tlasWrite.accelerationStructureCount = 1;
	tlasWrite.pAccelerationStructures = &resources.tlas->accelerationStructure;

	VkDescriptorImageInfo renderResultInfo = VkDescriptorImageInfo{};
	renderResultInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	renderResultInfo.imageView = renderResult->view;
	renderResultInfo.sampler = VK_NULL_HANDLE;

	(*rtDescriptorSet)
		.writeEXT(0, &tlasWrite)
		.writeImage(1, &renderResultInfo)
		.flush();
}

void PathTracingPass::drawFrame(hri::ActiveFrame& frame, CommonResources& resources)
{
	debug.resetTimer();
	debug.cmdBeginLabel(frame.commandBuffer, "Path Tracing Pass");
	debug.cmdRecordStartTimestamp(frame.commandBuffer);

	VkImageMemoryBarrier2 renderResultBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
	renderResultBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR;
	renderResultBarrier.dstStageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
	renderResultBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
	renderResultBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
	renderResultBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	renderResultBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	renderResultBarrier.image = renderResult->image;
	renderResultBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	renderResultBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	renderResultBarrier.subresourceRange = hri::ImageResource::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);
	frame.pipelineBarrier({ renderResultBarrier });

	VkStridedDeviceAddressRegionKHR raygen = m_SBT->getRegion(raytracing::ShaderBindingTable::SGRayGen);
	VkStridedDeviceAddressRegionKHR miss = m_SBT->getRegion(raytracing::ShaderBindingTable::SGMiss);
	VkStridedDeviceAddressRegionKHR hit = m_SBT->getRegion(raytracing::ShaderBindingTable::SGHit);
	VkStridedDeviceAddressRegionKHR call = m_SBT->getRegion(raytracing::ShaderBindingTable::SGCall);

	PushConstantData pushConstants = PushConstantData{};
	pushConstants.frameIdx = resources.frameIndex;

	vkCmdPushConstants(
		frame.commandBuffer,
		m_layout,
		VK_SHADER_STAGE_RAYGEN_BIT_KHR,
		0, sizeof(PathTracingPass::PushConstantData),
		&pushConstants
	);

	VkDescriptorSet sets[] = { sceneDescriptorSet->set, rtDescriptorSet->set, };
	vkCmdBindDescriptorSets(
		frame.commandBuffer,
		m_pPSO->bindPoint,
		m_layout,
		0, 2, sets,
		0, nullptr
	);

	vkCmdBindPipeline(
		frame.commandBuffer,
		m_pPSO->bindPoint,
		m_pPSO->pipeline
	);

	rtContext.rayTracingDispatch.vkCmdTraceRays(
		frame.commandBuffer,
		&raygen,
		&miss,
		&hit,
		&call,
		context.swapchain.extent.width,
		context.swapchain.extent.height,
		1
	);

	renderResultBarrier.srcStageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
	renderResultBarrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
	renderResultBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
	renderResultBarrier.dstAccessMask = 0;
	renderResultBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	renderResultBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	frame.pipelineBarrier({ renderResultBarrier });

	debug.cmdRecordEndTimestamp(frame.commandBuffer);
	debug.cmdEndLabel(frame.commandBuffer);
}

void PathTracingPass::recreateResources(VkExtent2D resolution)
{
	renderResult = std::unique_ptr<hri::ImageResource>(new hri::ImageResource(
		context,
		VK_IMAGE_TYPE_2D,
		VK_FORMAT_R8G8B8A8_SNORM,
		VK_SAMPLE_COUNT_1_BIT,
		VkExtent3D{ resolution.width, resolution.height, 1 },
		1,
		1,
		VK_IMAGE_USAGE_STORAGE_BIT
		| VK_IMAGE_USAGE_SAMPLED_BIT
	));

	renderResult->createView(VK_IMAGE_VIEW_TYPE_2D, hri::ImageResource::DefaultComponentMapping(), hri::ImageResource::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1));
}

// --- PRESENT PASS ---

PresentPass::PresentPass(hri::RenderContext& ctx, hri::ShaderDatabase& shaderDB, hri::DescriptorSetAllocator& descriptorAllocator)
	:
	IRenderPass(ctx)
{
	// Set up input sampler
	passInputSampler = std::unique_ptr<hri::ImageSampler>(new hri::ImageSampler(
		context,
		VK_FILTER_LINEAR,
		VK_FILTER_LINEAR,
		VK_SAMPLER_MIPMAP_MODE_LINEAR
	));

	// Set up descriptor sets
	hri::DescriptorSetLayoutBuilder presentDescriptorSetLayoutBuilder(context);
	presentDescriptorSetLayoutBuilder
		.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

	presentDescriptorSetLayout = std::make_unique<hri::DescriptorSetLayout>(presentDescriptorSetLayoutBuilder.build());
	presentDescriptorSet = std::unique_ptr<hri::DescriptorSetManager>(new hri::DescriptorSetManager(context, descriptorAllocator, *presentDescriptorSetLayout));

	// Set up render pass
	hri::RenderPassBuilder passBuilder(ctx);
	passBuilder
		.addAttachment(
			ctx.swapFormat(), VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_UNDEFINED
		)
		.setAttachmentReference(hri::AttachmentType::Color, VkAttachmentReference{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

	passResources = std::make_unique<hri::SwapchainPassResourceManager>(ctx, passBuilder.build());
	passResources->setClearValue(0, VkClearValue{ { 0.0f, 0.0f, 0.0f, 0.0f } });

	// Set up render pipeline
	hri::PipelineLayoutBuilder layoutBuilder(context);
	m_layout = layoutBuilder
		.addDescriptorSetLayout(*presentDescriptorSetLayout)
		.build();

	shaderDB.registerShader("FullscreenQuadVert", hri::Shader::loadFile(context, "shaders/fullscreen_quad.vert.spv", VK_SHADER_STAGE_VERTEX_BIT));
	shaderDB.registerShader("PresentFrag", hri::Shader::loadFile(context, "shaders/present.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT));

	VkExtent2D swapExtent = context.swapchain.extent;
	std::vector<VkPipelineColorBlendAttachmentState> blendAttachments = {
		VkPipelineColorBlendAttachmentState{
			false,
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
		},
	};

	hri::GraphicsPipelineBuilder pipelineBuilder = hri::GraphicsPipelineBuilder{};
	pipelineBuilder.vertexInputBindings = {};
	pipelineBuilder.vertexInputAttributes = {};
	pipelineBuilder.inputAssemblyState = hri::GraphicsPipelineBuilder::initInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, false);
	pipelineBuilder.viewport = hri::GraphicsPipelineBuilder::initDefaultViewport(static_cast<float>(swapExtent.width), static_cast<float>(swapExtent.height));
	pipelineBuilder.scissor = hri::GraphicsPipelineBuilder::initDefaultScissor(swapExtent.width, swapExtent.height);
	pipelineBuilder.rasterizationState = hri::GraphicsPipelineBuilder::initRasterizationState(false, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	pipelineBuilder.multisampleState = hri::GraphicsPipelineBuilder::initMultisampleState(VK_SAMPLE_COUNT_1_BIT);
	pipelineBuilder.depthStencilState = hri::GraphicsPipelineBuilder::initDepthStencilState(false, false);
	pipelineBuilder.colorBlendState = hri::GraphicsPipelineBuilder::initColorBlendState(blendAttachments);
	pipelineBuilder.dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	pipelineBuilder.layout = m_layout;
	pipelineBuilder.renderPass = passResources->renderPass();
	pipelineBuilder.subpass = 0;

	m_pPSO = shaderDB.createPipeline("PresentPipeline", { "FullscreenQuadVert", "PresentFrag"}, pipelineBuilder);
}

PresentPass::~PresentPass()
{
	vkDestroyPipelineLayout(context.device, m_layout, nullptr);
}

void PresentPass::drawFrame(hri::ActiveFrame& frame, CommonResources& resources)
{
	passResources->beginRenderPass(frame);

	debug.resetTimer();
	debug.cmdBeginLabel(frame.commandBuffer, "Present Pass");
	debug.cmdRecordStartTimestamp(frame.commandBuffer);

	VkExtent2D swapExtent = context.swapchain.extent;
	VkViewport viewport = VkViewport{ 0.0f, 0.0f, static_cast<float>(swapExtent.width), static_cast<float>(swapExtent.height), hri::DefaultViewportMinDepth, hri::DefaultViewportMaxDepth };
	VkRect2D scissor = VkRect2D{ VkOffset2D{0, 0}, swapExtent };

	vkCmdSetViewport(frame.commandBuffer, 0, 1, &viewport);
	vkCmdSetScissor(frame.commandBuffer, 0, 1, &scissor);

	vkCmdBindDescriptorSets(
		frame.commandBuffer,
		m_pPSO->bindPoint,
		m_layout,
		0, 1, &presentDescriptorSet->set,
		0, nullptr
	);

	vkCmdBindPipeline(
		frame.commandBuffer,
		m_pPSO->bindPoint,
		m_pPSO->pipeline
	);

	vkCmdDraw(frame.commandBuffer, 3, 1, 0, 0);

	debug.cmdRecordEndTimestamp(frame.commandBuffer);
	debug.cmdEndLabel(frame.commandBuffer);

	passResources->endRenderPass(frame);
}

// --- UI PASS ---

UIPass::UIPass(hri::RenderContext& ctx, VkDescriptorPool uiPool)
	:
	IRenderPass(ctx)
{
	hri::RenderPassBuilder passBuilder(ctx);
	passBuilder
		.addAttachment(
			ctx.swapFormat(), VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		)
		.setAttachmentReference(hri::AttachmentType::Color, VkAttachmentReference{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

	passResources = std::make_unique<hri::SwapchainPassResourceManager>(ctx, passBuilder.build());

	ImGui_ImplVulkan_InitInfo initInfo = {};
	initInfo.Instance = ctx.instance;
	initInfo.PhysicalDevice = ctx.gpu;
	initInfo.Device = ctx.device;
	initInfo.QueueFamily = ctx.queues.graphicsQueue.family;
	initInfo.Queue = ctx.queues.graphicsQueue.handle;
	initInfo.PipelineCache = VK_NULL_HANDLE;
	initInfo.DescriptorPool = uiPool;
	initInfo.Subpass = 0;
	initInfo.MinImageCount = ctx.swapchain.requested_min_image_count;
	initInfo.ImageCount = ctx.swapchain.image_count;
	initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	initInfo.RenderPass = passResources->renderPass();
	ImGui_ImplVulkan_Init(&initInfo);
}

UIPass::~UIPass()
{
	ImGui_ImplVulkan_Shutdown();
}

void UIPass::drawFrame(hri::ActiveFrame& frame, CommonResources& resources)
{
	passResources->beginRenderPass(frame);

	debug.resetTimer();
	debug.cmdBeginLabel(frame.commandBuffer, "UI Pass");
	debug.cmdRecordStartTimestamp(frame.commandBuffer);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), frame.commandBuffer);

	debug.cmdRecordEndTimestamp(frame.commandBuffer);
	debug.cmdEndLabel(frame.commandBuffer);

	passResources->endRenderPass(frame);
}
