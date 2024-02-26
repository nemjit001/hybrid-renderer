#include "renderer.h"

#include <hybrid_renderer.h>
#include <memory>

#include "detail/raytracing.h"
#include "subsystems.h"

Renderer::Renderer(raytracing::RayTracingContext& ctx, hri::Camera& camera, SceneGraph& activeScene)
	:
	m_context(ctx.renderContext),
	m_raytracingContext(ctx),
	m_renderCore(m_context),
	m_shaderDatabase(m_context),
	m_subsystemManager(),
	m_descriptorSetAllocator(m_context),
	m_stagingPool(m_context, m_context.queues.transferQueue, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT),
	m_camera(camera),
	m_activeScene(activeScene)
{
	initShaderDB();
	initRenderPasses();
	initSharedResources();
	initGlobalDescriptorSets();
	initRenderSubsystems();
	initRendererFrameData();

	m_renderCore.setOnSwapchainInvalidateCallback([this](const vkb::Swapchain& _swapchain) {
		recreateSwapDependentResources();
	});

	// Prepare first frame resources
	prepareFrameResources(0);
}

Renderer::~Renderer()
{
	m_renderCore.awaitFrameFinished();
}

void Renderer::setVSyncMode(hri::VSyncMode vsyncMode)
{
	m_context.setVSyncMode(vsyncMode);
	recreateSwapDependentResources();
}

void Renderer::drawFrame()
{
	m_renderCore.startFrame();
	hri::ActiveFrame frame = m_renderCore.getActiveFrame();

	// Prepare next frame's resources
	uint32_t nextFrameIdx = (frame.currentFrameIndex + 1) % hri::RenderCore::framesInFlight();
	prepareFrameResources(nextFrameIdx);

	// Begin command recording for this frame
	frame.beginCommands();

	// Record GBufferLayout pass
	m_gbufferLayoutPassManager->beginRenderPass(frame);
	m_subsystemManager.recordSubsystem("GBufferLayoutSystem", frame);
	m_gbufferLayoutPassManager->endRenderPass(frame);

	// Transition GBuffer output resources
	{
		VkImageMemoryBarrier2 gbufferAlbedoBarrier = VkImageMemoryBarrier2{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
		gbufferAlbedoBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		gbufferAlbedoBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
		gbufferAlbedoBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		gbufferAlbedoBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
		gbufferAlbedoBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		gbufferAlbedoBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		gbufferAlbedoBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		gbufferAlbedoBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		gbufferAlbedoBarrier.image = m_gbufferLayoutPassManager->getAttachmentResource(0).image;
		gbufferAlbedoBarrier.subresourceRange = VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

		VkImageMemoryBarrier2 gbufferWPosBarrier = VkImageMemoryBarrier2{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
		gbufferWPosBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		gbufferWPosBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
		gbufferWPosBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		gbufferWPosBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
		gbufferWPosBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		gbufferWPosBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		gbufferWPosBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		gbufferWPosBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		gbufferWPosBarrier.image = m_gbufferLayoutPassManager->getAttachmentResource(1).image;
		gbufferWPosBarrier.subresourceRange = VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

		VkImageMemoryBarrier2 gbufferNormalBarrier = VkImageMemoryBarrier2{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
		gbufferNormalBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		gbufferNormalBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
		gbufferNormalBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		gbufferNormalBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
		gbufferNormalBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		gbufferNormalBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		gbufferNormalBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		gbufferNormalBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		gbufferNormalBarrier.image = m_gbufferLayoutPassManager->getAttachmentResource(2).image;
		gbufferNormalBarrier.subresourceRange = VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

		VkImageMemoryBarrier2 gbufferDepthBarrier = VkImageMemoryBarrier2{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
		gbufferDepthBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		gbufferDepthBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
		gbufferDepthBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		gbufferDepthBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
		gbufferDepthBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		gbufferDepthBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		gbufferDepthBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		gbufferDepthBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		gbufferDepthBarrier.image = m_gbufferLayoutPassManager->getAttachmentResource(3).image;
		gbufferDepthBarrier.subresourceRange = VkImageSubresourceRange{ VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1 };

		frame.pipelineBarrier({ gbufferAlbedoBarrier, gbufferWPosBarrier, gbufferNormalBarrier, gbufferDepthBarrier });
	}
	
	// Transition Raytracing targets to storage layouts
	{
		VkImageMemoryBarrier2 softShadowGeneralBarrier = VkImageMemoryBarrier2{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
		softShadowGeneralBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		softShadowGeneralBarrier.dstStageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
		softShadowGeneralBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		softShadowGeneralBarrier.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT_KHR;
		softShadowGeneralBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		softShadowGeneralBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		softShadowGeneralBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		softShadowGeneralBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		softShadowGeneralBarrier.image = m_softShadowRTPassResult->image;
		softShadowGeneralBarrier.subresourceRange = VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

		frame.pipelineBarrier({ softShadowGeneralBarrier });
	}

	// Execute raytracing passes
	m_subsystemManager.recordSubsystem("SoftShadowsRTSystem", frame);

	// Transition Raytracing targets to sample layouts
	{
		VkImageMemoryBarrier2 softShadowSampleBarrier = VkImageMemoryBarrier2{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
		softShadowSampleBarrier.srcStageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
		softShadowSampleBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
		softShadowSampleBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT_KHR;
		softShadowSampleBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
		softShadowSampleBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		softShadowSampleBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		softShadowSampleBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		softShadowSampleBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		softShadowSampleBarrier.image = m_softShadowRTPassResult->image;
		softShadowSampleBarrier.subresourceRange = VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

		frame.pipelineBarrier({ softShadowSampleBarrier });
	}

	// Record swapchain output passes
	m_swapchainPassManager->beginRenderPass(frame);
	m_subsystemManager.recordSubsystem("PresentationSystem", frame);
	m_subsystemManager.recordSubsystem("UISystem", frame);	// Must be drawn after present as overlay
	m_swapchainPassManager->endRenderPass(frame);

	frame.endCommands();
	m_renderCore.endFrame();
}

void Renderer::initShaderDB()
{
	// Raytracing Shaders
	m_shaderDatabase.registerShader("HybridRayGen", hri::Shader::loadFile(m_context, "./shaders/hybrid.rgen.spv", VK_SHADER_STAGE_RAYGEN_BIT_KHR));
	m_shaderDatabase.registerShader("SoftShadowMiss", hri::Shader::loadFile(m_context, "./shaders/soft_shadow.rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR));
	m_shaderDatabase.registerShader("SoftShadowClosestHit", hri::Shader::loadFile(m_context, "./shaders/soft_shadow.rchit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR));


	// Vertex shaders
	m_shaderDatabase.registerShader("PresentVert", hri::Shader::loadFile(m_context, "./shaders/present.vert.spv", VK_SHADER_STAGE_VERTEX_BIT));
	m_shaderDatabase.registerShader("StaticVert", hri::Shader::loadFile(m_context, "./shaders/static.vert.spv", VK_SHADER_STAGE_VERTEX_BIT));

	// Fragment shaders
	m_shaderDatabase.registerShader("GBufferLayoutFrag", hri::Shader::loadFile(m_context, "./shaders/gbuffer_layout.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT));
	m_shaderDatabase.registerShader("PresentFrag", hri::Shader::loadFile(m_context, "./shaders/present.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT));
}

void Renderer::initRenderPasses()
{
	// GBuffer Layout pass
	{
		hri::RenderPassBuilder gbufferLayoutPassBuilder = hri::RenderPassBuilder(m_context)
			.addAttachment(
				VK_FORMAT_R8G8B8A8_UNORM,
				VK_SAMPLE_COUNT_1_BIT,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_ATTACHMENT_LOAD_OP_CLEAR,
				VK_ATTACHMENT_STORE_OP_STORE
			)
			.addAttachment(
				VK_FORMAT_R8G8B8A8_SNORM,
				VK_SAMPLE_COUNT_1_BIT,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_ATTACHMENT_LOAD_OP_CLEAR,
				VK_ATTACHMENT_STORE_OP_STORE
			)
			.addAttachment(
				VK_FORMAT_R8G8B8A8_SNORM,
				VK_SAMPLE_COUNT_1_BIT,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_ATTACHMENT_LOAD_OP_CLEAR,
				VK_ATTACHMENT_STORE_OP_STORE
			)
			.addAttachment(
				VK_FORMAT_D24_UNORM_S8_UINT,
				VK_SAMPLE_COUNT_1_BIT,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
				VK_ATTACHMENT_LOAD_OP_CLEAR,
				VK_ATTACHMENT_STORE_OP_STORE
			)
			.setAttachmentReference(
				hri::AttachmentType::Color,
				VkAttachmentReference{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }
			)
			.setAttachmentReference(
				hri::AttachmentType::Color,
				VkAttachmentReference{ 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }
			)
			.setAttachmentReference(
				hri::AttachmentType::Color,
				VkAttachmentReference{ 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }
			)
			.setAttachmentReference(
				hri::AttachmentType::DepthStencil,
				VkAttachmentReference{ 3, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL }
			);

		const std::vector<hri::RenderAttachmentConfig> gbufferAttachmentConfigs = {
			hri::RenderAttachmentConfig{ // Albedo target
				VK_FORMAT_R8G8B8A8_UNORM,
				VK_SAMPLE_COUNT_1_BIT,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
				| VK_IMAGE_USAGE_SAMPLED_BIT,
				VK_IMAGE_ASPECT_COLOR_BIT
			},
			hri::RenderAttachmentConfig{ // wPos target
				VK_FORMAT_R8G8B8A8_SNORM,
				VK_SAMPLE_COUNT_1_BIT,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
				| VK_IMAGE_USAGE_SAMPLED_BIT,
				VK_IMAGE_ASPECT_COLOR_BIT
			},
			hri::RenderAttachmentConfig{ // Normal target
				VK_FORMAT_R8G8B8A8_SNORM,
				VK_SAMPLE_COUNT_1_BIT,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
				| VK_IMAGE_USAGE_SAMPLED_BIT,
				VK_IMAGE_ASPECT_COLOR_BIT
			},
			hri::RenderAttachmentConfig{ // Depth Stencil target
				VK_FORMAT_D24_UNORM_S8_UINT,
				VK_SAMPLE_COUNT_1_BIT,
				VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
				| VK_IMAGE_USAGE_SAMPLED_BIT,
				VK_IMAGE_ASPECT_DEPTH_BIT
				| VK_IMAGE_ASPECT_STENCIL_BIT
			},
		};

		m_gbufferLayoutPassManager = std::unique_ptr<hri::RenderPassResourceManager>(new hri::RenderPassResourceManager(
			m_context,
			gbufferLayoutPassBuilder.build(),
			gbufferAttachmentConfigs
		));
	}

	// Swapchain pass pass
	{
		hri::RenderPassBuilder swapchainPassBuilder = hri::RenderPassBuilder(m_context)
			.addAttachment(
				m_context.swapFormat(),
				VK_SAMPLE_COUNT_1_BIT,
				VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				VK_ATTACHMENT_LOAD_OP_CLEAR,
				VK_ATTACHMENT_STORE_OP_STORE
			)
			.setAttachmentReference(
				hri::AttachmentType::Color,
				VkAttachmentReference{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }
			);

		m_swapchainPassManager = std::unique_ptr<hri::SwapchainPassResourceManager>(new hri::SwapchainPassResourceManager(
			m_context,
			swapchainPassBuilder.build()
		));
	}

	m_gbufferLayoutPassManager->setClearValue(0, VkClearValue{ { 0.0f, 0.0f, 0.0f, 0.0f } });
	m_gbufferLayoutPassManager->setClearValue(1, VkClearValue{ { 0.0f, 0.0f, 0.0f, 0.0f } });
	m_gbufferLayoutPassManager->setClearValue(2, VkClearValue{ { 0.0f, 0.0f, 0.0f, 0.0f } });
	m_gbufferLayoutPassManager->setClearValue(3, VkClearValue{ { 1.0f, 0x00 } });

	m_swapchainPassManager->setClearValue(0, VkClearValue{ { 0.0f, 0.0f, 0.0f, 1.0f } });
}

void Renderer::initSharedResources()
{
	VkExtent2D swapExtent = m_context.swapchain.extent;

	// init shared samplers
	m_renderResultNearestSampler = std::unique_ptr<hri::ImageSampler>(new hri::ImageSampler(
		m_context,
		VK_FILTER_NEAREST,
		VK_FILTER_NEAREST,
		VK_SAMPLER_MIPMAP_MODE_NEAREST
	));

	m_renderResultLinearSampler = std::unique_ptr<hri::ImageSampler>(new hri::ImageSampler(
		m_context,
		VK_FILTER_LINEAR,
		VK_FILTER_LINEAR,
		VK_SAMPLER_MIPMAP_MODE_LINEAR
	));

	// Init raytracing targets
	m_softShadowRTPassResult = std::unique_ptr<hri::ImageResource>(new hri::ImageResource(
		m_context,
		VK_IMAGE_TYPE_2D,
		VK_FORMAT_R8_UNORM,
		VK_SAMPLE_COUNT_1_BIT,
		{ swapExtent.width, swapExtent.height, 1 },
		1,
		1,
		VK_IMAGE_USAGE_STORAGE_BIT
		| VK_IMAGE_USAGE_SAMPLED_BIT
	));

	// Initialize raytracing target views
	m_softShadowRTPassResult->createView(
		VK_IMAGE_VIEW_TYPE_2D,
		hri::ImageResource::DefaultComponentMapping(),
		VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
	);
}

void Renderer::initGlobalDescriptorSets()
{
	hri::DescriptorSetLayoutBuilder sceneDataSetBuilder = hri::DescriptorSetLayoutBuilder(m_context)
		.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);	// Camera data

	hri::DescriptorSetLayoutBuilder raytracingTargetSetBuilder = hri::DescriptorSetLayoutBuilder(m_context)
		.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR);	// Soft shadow result

	hri::DescriptorSetLayoutBuilder presentInputSetBuilder = hri::DescriptorSetLayoutBuilder(m_context)
		.addBinding(PresentationSubsystem::renderResultBinding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

	m_sceneDataSetLayout = std::unique_ptr<hri::DescriptorSetLayout>(new hri::DescriptorSetLayout(sceneDataSetBuilder.build()));
	m_raytracingTargetSetLayout = std::unique_ptr<hri::DescriptorSetLayout>(new hri::DescriptorSetLayout(raytracingTargetSetBuilder.build()));
	m_presentInputSetLayout = std::unique_ptr<hri::DescriptorSetLayout>(new hri::DescriptorSetLayout(presentInputSetBuilder.build()));
}

void Renderer::initRenderSubsystems()
{
	m_gbufferLayoutSubsystem = std::unique_ptr<GBufferLayoutSubsystem>(new GBufferLayoutSubsystem(
		m_context,
		m_shaderDatabase,
		m_gbufferLayoutPassManager->renderPass(),
		m_sceneDataSetLayout->setLayout
	));

	m_softShadowsRTSubsystem = std::unique_ptr<SoftShadowsRTSubsystem>(new SoftShadowsRTSubsystem(
		m_raytracingContext,
		m_shaderDatabase
	));

	m_uiSubsystem = std::unique_ptr<UISubsystem>(new UISubsystem(
		m_context,
		m_swapchainPassManager->renderPass(),
		m_descriptorSetAllocator.fixedPool()
	));

	m_presentSubsystem = std::unique_ptr<PresentationSubsystem>(new PresentationSubsystem(
		m_context,
		m_shaderDatabase,
		m_swapchainPassManager->renderPass(),
		m_presentInputSetLayout->setLayout
	));

	m_subsystemManager.registerSubsystem("GBufferLayoutSystem", m_gbufferLayoutSubsystem.get());
	m_subsystemManager.registerSubsystem("SoftShadowsRTSystem", m_softShadowsRTSubsystem.get());
	m_subsystemManager.registerSubsystem("UISystem", m_uiSubsystem.get());
	m_subsystemManager.registerSubsystem("PresentationSystem", m_presentSubsystem.get());
}

void Renderer::initRendererFrameData()
{
	for (size_t i = 0; i < hri::RenderCore::framesInFlight(); i++)
	{
		RendererFrameData& frame = m_frames[i];
		frame.cameraUBO = std::unique_ptr<hri::BufferResource>(new hri::BufferResource(
			m_context,
			sizeof(hri::CameraShaderData),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			true
		));

		frame.presentInputSet = std::unique_ptr<hri::DescriptorSetManager>(new hri::DescriptorSetManager(
			m_context,
			m_descriptorSetAllocator,
			*m_presentInputSetLayout
		));

		frame.sceneDataSet = std::unique_ptr<hri::DescriptorSetManager>(new hri::DescriptorSetManager(
			m_context,
			m_descriptorSetAllocator,
			*m_sceneDataSetLayout
		));
	}
}

void Renderer::recreateSwapDependentResources()
{
	VkExtent2D swapExtent = m_context.swapchain.extent;

	m_gbufferLayoutPassManager->recreateResources();
	m_swapchainPassManager->recreateResources();

	// Recreate raytracing targets
	m_softShadowRTPassResult = std::unique_ptr<hri::ImageResource>(new hri::ImageResource(
		m_context,
		m_softShadowRTPassResult->imageType,
		m_softShadowRTPassResult->format,	// Reuse format, that's fine
		m_softShadowRTPassResult->samples,
		{ swapExtent.width, swapExtent.height, 1 },
		1,
		1,
		m_softShadowRTPassResult->usage
	));

	// Recreate raytracing target views
	m_softShadowRTPassResult->createView(
		VK_IMAGE_VIEW_TYPE_2D,
		hri::ImageResource::DefaultComponentMapping(),
		VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
	);
}

void Renderer::prepareFrameResources(uint32_t frameIdx)
{
	assert(frameIdx < hri::RenderCore::framesInFlight());

	RendererFrameData& rendererFrameData = m_frames[frameIdx];
	rendererFrameData.instances = m_activeScene.generateInstanceData(m_camera);

	// Update UBO staging buffers
	hri::CameraShaderData cameraData = m_camera.getShaderData();
	rendererFrameData.cameraUBO->copyToBuffer(&cameraData, sizeof(hri::CameraShaderData));

	// Upload UBO staging data
	{
		VkCommandBuffer uploadCommandBuffer = m_stagingPool.createCommandBuffer(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

		//

		m_stagingPool.submitAndWait(uploadCommandBuffer);
	}

	// Update subsystem frame info
	m_gbufferLayoutSubsystem->updateFrameInfo(GBufferLayoutFrameInfo{
		rendererFrameData.sceneDataSet->set,
		rendererFrameData.instances,
	});

	m_presentSubsystem->updateFrameInfo(PresentFrameInfo{
		rendererFrameData.presentInputSet->set,
	});

	// Update per frame descriptors
	VkDescriptorBufferInfo cameraUBOInfo = VkDescriptorBufferInfo{};
	cameraUBOInfo.buffer = rendererFrameData.cameraUBO->buffer;
	cameraUBOInfo.offset = 0;
	cameraUBOInfo.range = rendererFrameData.cameraUBO->bufferSize;

	(*rendererFrameData.sceneDataSet)
		.writeBuffer(0, &cameraUBOInfo)
		.flush();

	VkDescriptorImageInfo gbufferAlbedoResult = VkDescriptorImageInfo{};
	gbufferAlbedoResult.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	gbufferAlbedoResult.imageView = m_gbufferLayoutPassManager->getAttachmentResource(0).view;
	gbufferAlbedoResult.sampler = m_renderResultLinearSampler->sampler;

	(*rendererFrameData.presentInputSet)
		.writeImage(PresentationSubsystem::renderResultBinding, &gbufferAlbedoResult)
		.flush();
}
