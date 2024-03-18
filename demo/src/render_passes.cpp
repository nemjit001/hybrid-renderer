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

// --- RNG Generation Pass ---

RngGenerationPass::RngGenerationPass(hri::RenderContext& ctx, hri::ShaderDatabase& shaderDB, hri::DescriptorSetAllocator& descriptorAllocator)
	:
	IRenderPass(ctx)
{
	recreateResources(context.swapchain.extent);

	hri::DescriptorSetLayoutBuilder rngDescriptorSetLayoutBuilder(context);
	rngDescriptorSetLayoutBuilder
		.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);

	rngDescriptorSetLayout = std::make_unique<hri::DescriptorSetLayout>(rngDescriptorSetLayoutBuilder.build());
	rngDescriptorSet = std::unique_ptr<hri::DescriptorSetManager>(new hri::DescriptorSetManager(context, descriptorAllocator, *rngDescriptorSetLayout));

	hri::PipelineLayoutBuilder layoutBuilder(context);
	m_layout = layoutBuilder
		.addPushConstant(sizeof(RngGenerationPass::PushConstantData), VK_SHADER_STAGE_COMPUTE_BIT)
		.addDescriptorSetLayout(*rngDescriptorSetLayout)
		.build();

	shaderDB.registerShader("RNGGenCompute", hri::Shader::loadFile(context, "shaders/rng_gen.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT));
	m_pPSO = shaderDB.createPipeline("RNGGenComputePipeline", "RNGGenCompute", m_layout);
}

RngGenerationPass::~RngGenerationPass()
{
	vkDestroyPipelineLayout(context.device, m_layout, nullptr);
}

void RngGenerationPass::prepareFrame(CommonResources& resources)
{
	VkDescriptorImageInfo rngSourceInfo = VkDescriptorImageInfo{};
	rngSourceInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	rngSourceInfo.imageView = rngSource->view;
	rngSourceInfo.sampler = VK_NULL_HANDLE;

	(*rngDescriptorSet)
		.writeImage(0, &rngSourceInfo)
		.flush();
}

void RngGenerationPass::drawFrame(hri::ActiveFrame& frame, CommonResources& resources)
{
	debug.resetTimer();
	debug.cmdBeginLabel(frame.commandBuffer, "RNG Gen Compute Pass");
	debug.cmdRecordStartTimestamp(frame.commandBuffer);

	VkImageMemoryBarrier2 rngSourceBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
	rngSourceBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR;
	rngSourceBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	rngSourceBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
	rngSourceBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
	rngSourceBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	rngSourceBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	rngSourceBarrier.image = rngSource->image;
	rngSourceBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	rngSourceBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	rngSourceBarrier.subresourceRange = hri::ImageResource::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);
	frame.pipelineBarrier({ rngSourceBarrier });

	vkCmdBindPipeline(
		frame.commandBuffer,
		m_pPSO->bindPoint,
		m_pPSO->pipeline
	);

	PushConstantData pushConstants = PushConstantData{
		resources.frameIndex,
	};

	vkCmdPushConstants(
		frame.commandBuffer,
		m_layout,
		VK_SHADER_STAGE_COMPUTE_BIT,
		0, sizeof(RngGenerationPass::PushConstantData),
		&pushConstants
	);

	VkDescriptorSet sets[] = { rngDescriptorSet->set, };
	vkCmdBindDescriptorSets(
		frame.commandBuffer,
		m_pPSO->bindPoint,
		m_layout,
		0, 1, sets,
		0, nullptr
	);

	vkCmdDispatch(
		frame.commandBuffer,
		rngSource->extent.width,
		rngSource->extent.height,
		1
	);
	
	rngSourceBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	rngSourceBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	rngSourceBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
	rngSourceBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
	rngSourceBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	rngSourceBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	frame.pipelineBarrier({ rngSourceBarrier });

	debug.cmdRecordEndTimestamp(frame.commandBuffer);
	debug.cmdEndLabel(frame.commandBuffer);
}

void RngGenerationPass::recreateResources(VkExtent2D resolution)
{
	rngSource = std::unique_ptr<hri::ImageResource>(new hri::ImageResource(
		context,
		VK_IMAGE_TYPE_2D,
		VK_FORMAT_R32_SFLOAT,
		VK_SAMPLE_COUNT_1_BIT,
		VkExtent3D{ resolution.width, resolution.height, 1 },
		1,
		1,
		VK_IMAGE_USAGE_STORAGE_BIT
		| VK_IMAGE_USAGE_SAMPLED_BIT
	));

	rngSource->createView(VK_IMAGE_VIEW_TYPE_2D, hri::ImageResource::DefaultComponentMapping(), hri::ImageResource::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1));
}

// --- Path Tracing Pass ---

PathTracingPass::PathTracingPass(raytracing::RayTracingContext& ctx, hri::ShaderDatabase& shaderDB, hri::DescriptorSetAllocator& descriptorAllocator)
	:
	IRenderPass(ctx.renderContext),
	rtContext(ctx),
	pingPong(false)
{
	recreateResources(context.swapchain.extent);

	hri::DescriptorSetLayoutBuilder sceneDescriptorSetLayoutBuilder(context);
	sceneDescriptorSetLayoutBuilder
		.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.addBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.addBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
		.addBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);

	hri::DescriptorSetLayoutBuilder rtDescriptorSetLayoutBuilder(context);
	rtDescriptorSetLayoutBuilder
		.addBinding(0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
		.addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.addBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.addBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR);

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
	VkDescriptorBufferInfo currCameraInfo = VkDescriptorBufferInfo{};
	currCameraInfo.buffer = resources.cameraUBO->buffer;
	currCameraInfo.offset = 0;
	currCameraInfo.range = resources.cameraUBO->bufferSize;

	VkDescriptorBufferInfo prevCameraInfo = VkDescriptorBufferInfo{};
	prevCameraInfo.buffer = resources.prevCameraUBO->buffer;
	prevCameraInfo.offset = 0;
	prevCameraInfo.range = resources.prevCameraUBO->bufferSize;

	VkDescriptorBufferInfo instanceInfo = VkDescriptorBufferInfo{};
	instanceInfo.buffer = resources.instanceDataSSBO->buffer;
	instanceInfo.offset = 0;
	instanceInfo.range = resources.instanceDataSSBO->bufferSize;

	VkDescriptorBufferInfo materialInfo = VkDescriptorBufferInfo{};
	materialInfo.buffer = resources.materialSSBO->buffer;
	materialInfo.offset = 0;
	materialInfo.range = resources.materialSSBO->bufferSize;

	(*sceneDescriptorSet)
		.writeBuffer(0, &currCameraInfo)
		.writeBuffer(1, &prevCameraInfo)
		.writeBuffer(2, &instanceInfo)
		.writeBuffer(3, &materialInfo)
		.flush();

	VkWriteDescriptorSetAccelerationStructureKHR tlasWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR };
	tlasWrite.accelerationStructureCount = 1;
	tlasWrite.pAccelerationStructures = &resources.tlas->accelerationStructure;

	VkDescriptorImageInfo renderResultInfo = VkDescriptorImageInfo{};
	renderResultInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	renderResultInfo.imageView = renderResult[pingPong]->view;
	renderResultInfo.sampler = VK_NULL_HANDLE;

	VkDescriptorImageInfo prevResultInfo = VkDescriptorImageInfo{};
	prevResultInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	prevResultInfo.imageView = renderResult[!pingPong]->view;
	prevResultInfo.sampler = VK_NULL_HANDLE;

	VkDescriptorImageInfo reprojectHistoryInfo = VkDescriptorImageInfo{};
	reprojectHistoryInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	reprojectHistoryInfo.imageView = reprojectHistory->view;
	reprojectHistoryInfo.sampler = VK_NULL_HANDLE;

	(*rtDescriptorSet)
		.writeEXT(0, &tlasWrite)
		.writeImage(1, &renderResultInfo)
		.writeImage(2, &prevResultInfo)
		.writeImage(3, &reprojectHistoryInfo)
		.flush();
}

void PathTracingPass::drawFrame(hri::ActiveFrame& frame, CommonResources& resources)
{
	debug.resetTimer();
	debug.cmdBeginLabel(frame.commandBuffer, "Path Tracing Pass");
	debug.cmdRecordStartTimestamp(frame.commandBuffer);

	VkImageMemoryBarrier2 prevFrameResultBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
	prevFrameResultBarrier.srcStageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
	prevFrameResultBarrier.dstStageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
	prevFrameResultBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
	prevFrameResultBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
	prevFrameResultBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	prevFrameResultBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	prevFrameResultBarrier.image = renderResult[!pingPong]->image;
	prevFrameResultBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	prevFrameResultBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	prevFrameResultBarrier.subresourceRange = hri::ImageResource::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);

	VkImageMemoryBarrier2 renderResultBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
	renderResultBarrier.srcStageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
	renderResultBarrier.dstStageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
	renderResultBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
	renderResultBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
	renderResultBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	renderResultBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	renderResultBarrier.image = renderResult[pingPong]->image;
	renderResultBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	renderResultBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	renderResultBarrier.subresourceRange = hri::ImageResource::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);

	VkImageMemoryBarrier2 reprojectHistoryBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
	reprojectHistoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
	reprojectHistoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
	reprojectHistoryBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
	reprojectHistoryBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
	reprojectHistoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	reprojectHistoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	reprojectHistoryBarrier.image = reprojectHistory->image;
	reprojectHistoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	reprojectHistoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	reprojectHistoryBarrier.subresourceRange = hri::ImageResource::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);
	frame.pipelineBarrier({ prevFrameResultBarrier, renderResultBarrier, reprojectHistoryBarrier });

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
	renderResultBarrier.dstStageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
	renderResultBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
	renderResultBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
	renderResultBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	renderResultBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	frame.pipelineBarrier({ renderResultBarrier });

	debug.cmdRecordEndTimestamp(frame.commandBuffer);
	debug.cmdEndLabel(frame.commandBuffer);

	pingPong = !pingPong;
}

void PathTracingPass::recreateResources(VkExtent2D resolution)
{
	for (uint32_t i = 0; i < 2; i++)
	{
		renderResult[i] = std::unique_ptr<hri::ImageResource>(new hri::ImageResource(
			context,
			VK_IMAGE_TYPE_2D,
			VK_FORMAT_R32G32B32A32_SFLOAT,
			VK_SAMPLE_COUNT_1_BIT,
			VkExtent3D{ resolution.width, resolution.height, 1 },
			1,
			1,
			VK_IMAGE_USAGE_STORAGE_BIT
			| VK_IMAGE_USAGE_SAMPLED_BIT
		));

		renderResult[i]->createView(VK_IMAGE_VIEW_TYPE_2D, hri::ImageResource::DefaultComponentMapping(), hri::ImageResource::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1));
	};

	reprojectHistory = std::unique_ptr<hri::ImageResource>(new hri::ImageResource(
		context,
		VK_IMAGE_TYPE_2D,
		VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_SAMPLE_COUNT_1_BIT,
		VkExtent3D{ resolution.width, resolution.height, 1 },
		1,
		1,
		VK_IMAGE_USAGE_STORAGE_BIT
	));

	reprojectHistory->createView(VK_IMAGE_VIEW_TYPE_2D, hri::ImageResource::DefaultComponentMapping(), hri::ImageResource::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1));
}

// --- GBUFFER LAYOUT PASS ---

GBufferLayoutPass::GBufferLayoutPass(hri::RenderContext& ctx, hri::ShaderDatabase& shaderDB, hri::DescriptorSetAllocator& descriptorAllocator)
	:
	IRenderPass(ctx)
{
	// Set up descriptor set
	{
		hri::DescriptorSetLayoutBuilder sceneDescriptorSetLayoutBuilder(context);
		sceneDescriptorSetLayoutBuilder
			.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
			.addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.addBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);

		sceneDescriptorSetLayout = std::make_unique<hri::DescriptorSetLayout>(sceneDescriptorSetLayoutBuilder.build());
		sceneDescriptorSet = std::unique_ptr<hri::DescriptorSetManager>(new hri::DescriptorSetManager(context, descriptorAllocator, *sceneDescriptorSetLayout));
	}

	// Set up render pass
	{
		hri::RenderPassBuilder passBuilder(context);
		passBuilder
			.addAttachment( // Albedo target
				VK_FORMAT_R8G8B8A8_SNORM, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE
			)
			.addAttachment( // Emission target
				VK_FORMAT_R32G32B32A32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE
			)
			.addAttachment( // Specular target
				VK_FORMAT_R32G32B32A32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE
			)
			.addAttachment( // Transmittance target
				VK_FORMAT_R32G32B32A32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE
			)
			.addAttachment( // Normal target
				VK_FORMAT_R32G32B32A32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE
			)
			.addAttachment( // LOD Mask target (SFLOAT, but converted in sample pass to UINT32)
				VK_FORMAT_R32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE
			)
			.addAttachment( // Depth target
				VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE
			)
			.setAttachmentReference(hri::AttachmentType::Color, VkAttachmentReference{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL })
			.setAttachmentReference(hri::AttachmentType::Color, VkAttachmentReference{ 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL })
			.setAttachmentReference(hri::AttachmentType::Color, VkAttachmentReference{ 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL })
			.setAttachmentReference(hri::AttachmentType::Color, VkAttachmentReference{ 3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL })
			.setAttachmentReference(hri::AttachmentType::Color, VkAttachmentReference{ 4, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL })
			.setAttachmentReference(hri::AttachmentType::Color, VkAttachmentReference{ 5, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL })
			.setAttachmentReference(hri::AttachmentType::DepthStencil, VkAttachmentReference{ 6, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL });

		// Attachment configs
		std::vector<hri::RenderAttachmentConfig> attachmentConfigs = {
			hri::RenderAttachmentConfig{ VK_FORMAT_R8G8B8A8_SNORM, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT },
			hri::RenderAttachmentConfig{ VK_FORMAT_R32G32B32A32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT },
			hri::RenderAttachmentConfig{ VK_FORMAT_R32G32B32A32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT },
			hri::RenderAttachmentConfig{ VK_FORMAT_R32G32B32A32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT },
			hri::RenderAttachmentConfig{ VK_FORMAT_R32G32B32A32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT },
			hri::RenderAttachmentConfig{ VK_FORMAT_R32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT },
			hri::RenderAttachmentConfig{ VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_DEPTH_BIT },
		};

		loDefLODPassResources = std::make_unique<hri::RenderPassResourceManager>(ctx, passBuilder.build(), attachmentConfigs);
		hiDefLODPassResources = std::make_unique<hri::RenderPassResourceManager>(ctx, passBuilder.build(), attachmentConfigs);

		// lo def clear values
		loDefLODPassResources->setClearValue(0, VkClearValue{ { 0.0f, 0.0f, 0.0f, 0.0f } });
		loDefLODPassResources->setClearValue(1, VkClearValue{ { 0.0f, 0.0f, 0.0f, 0.0f } });
		loDefLODPassResources->setClearValue(2, VkClearValue{ { 0.0f, 0.0f, 0.0f, 0.0f } });
		loDefLODPassResources->setClearValue(3, VkClearValue{ { 0.0f, 0.0f, 0.0f, 0.0f } });
		loDefLODPassResources->setClearValue(4, VkClearValue{ { 0.0f, 0.0f, 0.0f, 0.0f } });
		loDefLODPassResources->setClearValue(5, VkClearValue{ { 0.0f, 0.0f, 0.0f, 0.0f } });
		loDefLODPassResources->setClearValue(6, VkClearValue{ { 1.0f, 0x00 } });

		// hi def clear values
		hiDefLODPassResources->setClearValue(0, VkClearValue{ { 0.0f, 0.0f, 0.0f, 0.0f } });
		hiDefLODPassResources->setClearValue(1, VkClearValue{ { 0.0f, 0.0f, 0.0f, 0.0f } });
		hiDefLODPassResources->setClearValue(2, VkClearValue{ { 0.0f, 0.0f, 0.0f, 0.0f } });
		hiDefLODPassResources->setClearValue(3, VkClearValue{ { 0.0f, 0.0f, 0.0f, 0.0f } });
		hiDefLODPassResources->setClearValue(4, VkClearValue{ { 0.0f, 0.0f, 0.0f, 0.0f } });
		hiDefLODPassResources->setClearValue(5, VkClearValue{ { 0.0f, 0.0f, 0.0f, 0.0f } });
		hiDefLODPassResources->setClearValue(6, VkClearValue{ { 1.0f, 0x00 } });
	}

	// Set up render pipeline
	{
		hri::PipelineLayoutBuilder layoutBuilder(context);
		m_layout = layoutBuilder
			.addPushConstant(sizeof(GBufferLayoutPass::PushConstantData), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
			.addDescriptorSetLayout(*sceneDescriptorSetLayout)
			.build();

		shaderDB.registerShader("StaticVert", hri::Shader::loadFile(context, "shaders/static.vert.spv", VK_SHADER_STAGE_VERTEX_BIT));
		shaderDB.registerShader("GBufferLayoutFrag", hri::Shader::loadFile(context, "shaders/gbuffer_layout.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT));

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
		pipelineBuilder.vertexInputBindings = { VkVertexInputBindingDescription{ 0, sizeof(hri::Vertex), VK_VERTEX_INPUT_RATE_VERTEX } };
		pipelineBuilder.vertexInputAttributes = {
			VkVertexInputAttributeDescription{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(hri::Vertex, position) },
			VkVertexInputAttributeDescription{ 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(hri::Vertex, normal) },
			VkVertexInputAttributeDescription{ 2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(hri::Vertex, tangent) },
			VkVertexInputAttributeDescription{ 3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(hri::Vertex, textureCoord) },
		};
		pipelineBuilder.inputAssemblyState = hri::GraphicsPipelineBuilder::initInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, false);
		pipelineBuilder.viewport = hri::GraphicsPipelineBuilder::initDefaultViewport(static_cast<float>(swapExtent.width), static_cast<float>(swapExtent.height));
		pipelineBuilder.scissor = hri::GraphicsPipelineBuilder::initDefaultScissor(swapExtent.width, swapExtent.height);
		pipelineBuilder.rasterizationState = hri::GraphicsPipelineBuilder::initRasterizationState(false, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
		pipelineBuilder.multisampleState = hri::GraphicsPipelineBuilder::initMultisampleState(VK_SAMPLE_COUNT_1_BIT);
		pipelineBuilder.depthStencilState = hri::GraphicsPipelineBuilder::initDepthStencilState(true, true, VK_COMPARE_OP_LESS);
		pipelineBuilder.colorBlendState = hri::GraphicsPipelineBuilder::initColorBlendState(blendAttachments);
		pipelineBuilder.dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		pipelineBuilder.layout = m_layout;
		pipelineBuilder.renderPass = loDefLODPassResources->renderPass();	// This is OK because lo & hi def both use the same render pass setup
		pipelineBuilder.subpass = 0;

		m_pPSO = shaderDB.createPipeline("GBufferLayoutPipeline", { "StaticVert", "GBufferLayoutFrag" }, pipelineBuilder);
	}
}

GBufferLayoutPass::~GBufferLayoutPass()
{
	vkDestroyPipelineLayout(context.device, m_layout, nullptr);
}

void GBufferLayoutPass::prepareFrame(CommonResources& resources)
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
}

void GBufferLayoutPass::drawFrame(hri::ActiveFrame& frame, CommonResources& resources)
{
	debug.resetTimer();
	debug.cmdRecordStartTimestamp(frame.commandBuffer);

	executeGBufferPass(*loDefLODPassResources, frame, resources, LODMode::LODFar);
	executeGBufferPass(*hiDefLODPassResources, frame, resources, LODMode::LODNear);

	VkMemoryBarrier2 memoryBarrier = VkMemoryBarrier2{ VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
	memoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	memoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	memoryBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	memoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
	frame.pipelineBarrier({ memoryBarrier });

	debug.cmdRecordEndTimestamp(frame.commandBuffer);
}

void GBufferLayoutPass::executeGBufferPass(hri::RenderPassResourceManager& pResourceManager, hri::ActiveFrame& frame, CommonResources& resources, LODMode mode)
{
	pResourceManager.beginRenderPass(frame);
	debug.cmdBeginLabel(frame.commandBuffer, (mode == LODMode::LODNear) ? "GBuffer Layout LOD Near" : "GBuffer Layout LOD Far");

	VkExtent2D swapExtent = context.swapchain.extent;
	VkViewport viewport = VkViewport{ 0.0f, 0.0f, static_cast<float>(swapExtent.width), static_cast<float>(swapExtent.height), hri::DefaultViewportMinDepth, hri::DefaultViewportMaxDepth };
	VkRect2D scissor = VkRect2D{ VkOffset2D{0, 0}, swapExtent };

	vkCmdSetViewport(frame.commandBuffer, 0, 1, &viewport);
	vkCmdSetScissor(frame.commandBuffer, 0, 1, &scissor);

	vkCmdBindDescriptorSets(
		frame.commandBuffer,
		m_pPSO->bindPoint,
		m_layout,
		0, 1, &sceneDescriptorSet->set,
		0, nullptr
	);

	vkCmdBindPipeline(
		frame.commandBuffer,
		m_pPSO->bindPoint,
		m_pPSO->pipeline
	);

	const auto instances = resources.activeScene->getRenderInstanceList();
	const bool useNearLOD = (mode == LODMode::LODNear);
	for (auto const& instance : instances)
	{
		// Get lod instance, lod mask, and mesh
		uint32_t instanceId = (useNearLOD) ? instance.instanceIdLOD0 : instance.instanceIdLOD1;
		uint32_t lodMask = SceneGraph::generateLODMask(instance);
		const hri::Mesh& mesh = resources.activeScene->meshes[instanceId];

		// Set up push constants
		PushConstantData pushConstants = PushConstantData{};
		pushConstants.instanceId = instanceId;
		pushConstants.lodMask = (useNearLOD) ? ((~lodMask) & VALID_MASK) : (lodMask & VALID_MASK);
		pushConstants.modelMatrix = instance.modelMatrix;

		vkCmdPushConstants(
			frame.commandBuffer,
			m_layout,
			VK_SHADER_STAGE_VERTEX_BIT
			| VK_SHADER_STAGE_FRAGMENT_BIT,
			0, sizeof(GBufferLayoutPass::PushConstantData),
			&pushConstants
		);

		VkDeviceSize offsets[] = {0};
		vkCmdBindVertexBuffers(frame.commandBuffer, 0, 1, &mesh.vertexBuffer.buffer, offsets);
		vkCmdBindIndexBuffer(frame.commandBuffer, mesh.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(frame.commandBuffer, mesh.indexCount, 1, 0, 0, 0);
	}

	debug.cmdEndLabel(frame.commandBuffer);
	pResourceManager.endRenderPass(frame);
}

// --- GBUFFER SAMPLER PASS ---

GBufferSamplePass::GBufferSamplePass(hri::RenderContext& context, hri::ShaderDatabase& shaderDB, hri::DescriptorSetAllocator& descriptorAllocator)
	:
	IRenderPass(context)
{
	// Set up input sampler
	passInputSampler = std::unique_ptr<hri::ImageSampler>(new hri::ImageSampler(
		context,
		VK_FILTER_NEAREST,
		VK_FILTER_NEAREST,
		VK_SAMPLER_MIPMAP_MODE_NEAREST
	));

	// Set up descriptor sets
	hri::DescriptorSetLayoutBuilder rngDescriptorSetLayoutBuilder(context);
	rngDescriptorSetLayoutBuilder
		.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

	hri::DescriptorSetLayoutBuilder gbufferSampleDescriptorSetLayoutBuilder(context);
	gbufferSampleDescriptorSetLayoutBuilder
		.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
		.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
		.addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
		.addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
		.addBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
		.addBinding(5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
		.addBinding(6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

	rngDescriptorSetLayout = std::make_unique<hri::DescriptorSetLayout>(rngDescriptorSetLayoutBuilder.build());
	gbufferSampleDescriptorSetLayout = std::make_unique<hri::DescriptorSetLayout>(gbufferSampleDescriptorSetLayoutBuilder.build());

	rngDescriptorSet = std::unique_ptr<hri::DescriptorSetManager>(new hri::DescriptorSetManager(context, descriptorAllocator, *rngDescriptorSetLayout));
	loDefDescriptorSet = std::unique_ptr<hri::DescriptorSetManager>(new hri::DescriptorSetManager(context, descriptorAllocator, *gbufferSampleDescriptorSetLayout));
	hiDefDescriptorSet = std::unique_ptr<hri::DescriptorSetManager>(new hri::DescriptorSetManager(context, descriptorAllocator, *gbufferSampleDescriptorSetLayout));

	// Set up render pass (almost the same as GBuffer layout)
	{
		hri::RenderPassBuilder passBuilder(context);
		passBuilder
			.addAttachment( // Albedo target
				VK_FORMAT_R8G8B8A8_SNORM, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE
			)
			.addAttachment( // Emission target
				VK_FORMAT_R32G32B32A32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE
			)
			.addAttachment( // Specular target
				VK_FORMAT_R32G32B32A32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE
			)
			.addAttachment( // Transmittance target
				VK_FORMAT_R32G32B32A32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE
			)
			.addAttachment( // Normal target
				VK_FORMAT_R32G32B32A32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE
			)
			.addAttachment( // Depth target
				VK_FORMAT_R32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE
			)
			.setAttachmentReference(hri::AttachmentType::Color, VkAttachmentReference{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL })
			.setAttachmentReference(hri::AttachmentType::Color, VkAttachmentReference{ 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL })
			.setAttachmentReference(hri::AttachmentType::Color, VkAttachmentReference{ 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL })
			.setAttachmentReference(hri::AttachmentType::Color, VkAttachmentReference{ 3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL })
			.setAttachmentReference(hri::AttachmentType::Color, VkAttachmentReference{ 4, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL })
			.setAttachmentReference(hri::AttachmentType::Color, VkAttachmentReference{ 5, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

		// Attachment configs
		std::vector<hri::RenderAttachmentConfig> attachmentConfigs = {
			hri::RenderAttachmentConfig{ VK_FORMAT_R8G8B8A8_SNORM, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT },
			hri::RenderAttachmentConfig{ VK_FORMAT_R32G32B32A32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT },
			hri::RenderAttachmentConfig{ VK_FORMAT_R32G32B32A32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT },
			hri::RenderAttachmentConfig{ VK_FORMAT_R32G32B32A32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT },
			hri::RenderAttachmentConfig{ VK_FORMAT_R32G32B32A32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT },
			hri::RenderAttachmentConfig{ VK_FORMAT_R32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT },
		};

		passResources = std::make_unique<hri::RenderPassResourceManager>(context, passBuilder.build(), attachmentConfigs);
		passResources->setClearValue(0, VkClearValue{ { 0.0f, 0.0f, 0.0f, 0.0f } });
		passResources->setClearValue(1, VkClearValue{ { 0.0f, 0.0f, 0.0f, 0.0f } });
		passResources->setClearValue(2, VkClearValue{ { 0.0f, 0.0f, 0.0f, 0.0f } });
		passResources->setClearValue(3, VkClearValue{ { 0.0f, 0.0f, 0.0f, 0.0f } });
		passResources->setClearValue(4, VkClearValue{ { 0.0f, 0.0f, 0.0f, 0.0f } });
		passResources->setClearValue(5, VkClearValue{ { 0.0f, 0.0f, 0.0f, 0.0f } });
	}

	// Set up render pipeline
	{
		hri::PipelineLayoutBuilder layoutBuilder(context);
		m_layout = layoutBuilder
			.addPushConstant(sizeof(GBufferSamplePass), VK_SHADER_STAGE_FRAGMENT_BIT)
			.addDescriptorSetLayout(*rngDescriptorSetLayout)
			.addDescriptorSetLayout(*gbufferSampleDescriptorSetLayout)
			.addDescriptorSetLayout(*gbufferSampleDescriptorSetLayout)
			.build();

		shaderDB.registerShader("FullscreenQuadVert", hri::Shader::loadFile(context, "shaders/fullscreen_quad.vert.spv", VK_SHADER_STAGE_VERTEX_BIT));
		shaderDB.registerShader("GBufferSampleFrag", hri::Shader::loadFile(context, "shaders/gbuffer_sample.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT));

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

		m_pPSO = shaderDB.createPipeline("GBufferSamplePipeline", { "FullscreenQuadVert", "GBufferSampleFrag" }, pipelineBuilder);
	}
}

GBufferSamplePass::~GBufferSamplePass()
{
	vkDestroyPipelineLayout(context.device, m_layout, nullptr);
}

void GBufferSamplePass::drawFrame(hri::ActiveFrame& frame, CommonResources& resources)
{
	passResources->beginRenderPass(frame);

	debug.resetTimer();
	debug.cmdBeginLabel(frame.commandBuffer, "GBuffer Sample Pass");
	debug.cmdRecordStartTimestamp(frame.commandBuffer);

	VkExtent2D swapExtent = context.swapchain.extent;
	VkViewport viewport = VkViewport{ 0.0f, 0.0f, static_cast<float>(swapExtent.width), static_cast<float>(swapExtent.height), hri::DefaultViewportMinDepth, hri::DefaultViewportMaxDepth };
	VkRect2D scissor = VkRect2D{ VkOffset2D{0, 0}, swapExtent };

	vkCmdSetViewport(frame.commandBuffer, 0, 1, &viewport);
	vkCmdSetScissor(frame.commandBuffer, 0, 1, &scissor);

	VkDescriptorSet sets[] = { rngDescriptorSet->set, loDefDescriptorSet->set, hiDefDescriptorSet->set, };
	vkCmdBindDescriptorSets(
		frame.commandBuffer,
		m_pPSO->bindPoint,
		m_layout,
		0, 3, sets,
		0, nullptr
	);

	PushConstantData pushConstants = PushConstantData{};
	pushConstants.resolution = hri::Float2((float)swapExtent.width, (float)swapExtent.height);

	vkCmdPushConstants(
		frame.commandBuffer,
		m_layout,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		0, sizeof(GBufferSamplePass::PushConstantData),
		&pushConstants
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

	VkMemoryBarrier2 memoryBarrier = VkMemoryBarrier2{ VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
	memoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	memoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	memoryBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	memoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
	frame.pipelineBarrier({ memoryBarrier });
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
			VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE
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
