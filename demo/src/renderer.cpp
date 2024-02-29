#include "renderer.h"

#include <hybrid_renderer.h>
#include <memory>

#include "detail/raytracing.h"
#include "subsystems.h"

Renderer::Renderer(raytracing::RayTracingContext& ctx, hri::Camera& camera, SceneGraph& activeScene)
	:
	m_context(ctx.renderContext),
	m_raytracingContext(ctx),
	m_debug(m_context),
	m_renderCore(m_context),
	m_shaderDatabase(m_context),
	m_subsystemManager(),
	m_descriptorSetAllocator(m_context),
	m_computePool(m_context, m_context.queues.computeQueue, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT),
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
		gbufferAlbedoBarrier.subresourceRange = hri::ImageResource::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);

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
		gbufferWPosBarrier.subresourceRange = hri::ImageResource::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);

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
		gbufferNormalBarrier.subresourceRange = hri::ImageResource::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);

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
		gbufferDepthBarrier.subresourceRange = hri::ImageResource::SubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1);

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
		softShadowGeneralBarrier.subresourceRange = hri::ImageResource::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);

		VkImageMemoryBarrier2 DIGeneralBarrier = VkImageMemoryBarrier2{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
		DIGeneralBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		DIGeneralBarrier.dstStageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
		DIGeneralBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		DIGeneralBarrier.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT_KHR;
		DIGeneralBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		DIGeneralBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		DIGeneralBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		DIGeneralBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		DIGeneralBarrier.image = m_directIlluminationRTPassResult->image;
		DIGeneralBarrier.subresourceRange = hri::ImageResource::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);

		frame.pipelineBarrier({ softShadowGeneralBarrier, DIGeneralBarrier });
	}

	// Execute raytracing passes
	m_subsystemManager.recordSubsystem("HybridRTSystem", frame);

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
		softShadowSampleBarrier.subresourceRange = hri::ImageResource::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);

		VkImageMemoryBarrier2 DISampleBarrier = VkImageMemoryBarrier2{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
		DISampleBarrier.srcStageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
		DISampleBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
		DISampleBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT_KHR;
		DISampleBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
		DISampleBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		DISampleBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		DISampleBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		DISampleBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		DISampleBarrier.image = m_directIlluminationRTPassResult->image;
		DISampleBarrier.subresourceRange = hri::ImageResource::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);

		frame.pipelineBarrier({ softShadowSampleBarrier, DISampleBarrier });
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
	m_shaderDatabase.registerShader("HybridMiss", hri::Shader::loadFile(m_context, "./shaders/hybrid.rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR));
	m_shaderDatabase.registerShader("HybridClosestHit", hri::Shader::loadFile(m_context, "./shaders/hybrid.rchit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR));


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

	m_directIlluminationRTPassResult = std::unique_ptr<hri::ImageResource>(new hri::ImageResource(
		m_context,
		VK_IMAGE_TYPE_2D,
		VK_FORMAT_R8G8B8A8_UNORM,
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
		hri::ImageResource::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1)
	);

	m_directIlluminationRTPassResult->createView(
		VK_IMAGE_VIEW_TYPE_2D,
		hri::ImageResource::DefaultComponentMapping(),
		hri::ImageResource::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1)
	);
}

void Renderer::initGlobalDescriptorSets()
{
	// Create set layouts
	hri::DescriptorSetLayoutBuilder sceneDataSetBuilder = hri::DescriptorSetLayoutBuilder(m_context)
		.addBinding(SceneDataBindings::Camera, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.addBinding(SceneDataBindings::RenderInstanceData, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
		.addBinding(SceneDataBindings::MaterialData, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);

	hri::DescriptorSetLayoutBuilder rtGlobalDescriptorSetBuilder = hri::DescriptorSetLayoutBuilder(m_context)
		.addBinding(RayTracingBindings::Tlas, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
		.addBinding(RayTracingBindings::GBufferAlbedo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.addBinding(RayTracingBindings::GBufferWorldPos, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.addBinding(RayTracingBindings::GBufferNormal, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.addBinding(RayTracingBindings::SoftShadowOutImage, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.addBinding(RayTracingBindings::DIOutImage, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR);

	hri::DescriptorSetLayoutBuilder presentInputSetBuilder = hri::DescriptorSetLayoutBuilder(m_context)
		.addBinding(PresentInputBindings::RenderResult, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

	// Allocate descriptor set layouts
	m_sceneDataSetLayout = std::unique_ptr<hri::DescriptorSetLayout>(new hri::DescriptorSetLayout(sceneDataSetBuilder.build()));
	m_rtDescriptorSetLayout = std::unique_ptr<hri::DescriptorSetLayout>(new hri::DescriptorSetLayout(rtGlobalDescriptorSetBuilder.build()));
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

	m_hybridRTSubsystem = std::unique_ptr<HybridRayTracingSubsystem>(new HybridRayTracingSubsystem(
		m_raytracingContext,
		m_shaderDatabase,
		m_sceneDataSetLayout->setLayout,
		m_rtDescriptorSetLayout->setLayout
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
	m_subsystemManager.registerSubsystem("HybridRTSystem", m_hybridRTSubsystem.get());
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

		frame.blasList = m_activeScene.accelStructManager.createBLASList(m_activeScene.meshes);

		frame.sceneDataSet = std::unique_ptr<hri::DescriptorSetManager>(new hri::DescriptorSetManager(
			m_context,
			m_descriptorSetAllocator,
			*m_sceneDataSetLayout
		));

		frame.raytracingSet = std::unique_ptr<hri::DescriptorSetManager>(new hri::DescriptorSetManager(
			m_context,
			m_descriptorSetAllocator,
			*m_rtDescriptorSetLayout
		));

		frame.presentInputSet = std::unique_ptr<hri::DescriptorSetManager>(new hri::DescriptorSetManager(
			m_context,
			m_descriptorSetAllocator,
			*m_presentInputSetLayout
		));

		updateFrameDescriptors(frame);
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

	m_directIlluminationRTPassResult = std::unique_ptr<hri::ImageResource>(new hri::ImageResource(
		m_context,
		m_directIlluminationRTPassResult->imageType,
		m_directIlluminationRTPassResult->format,
		m_directIlluminationRTPassResult->samples,
		{ swapExtent.width, swapExtent.height, 1 },
		1,
		1,
		m_directIlluminationRTPassResult->usage
	));

	// Recreate raytracing target views
	m_softShadowRTPassResult->createView(
		VK_IMAGE_VIEW_TYPE_2D,
		hri::ImageResource::DefaultComponentMapping(),
		hri::ImageResource::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1)
	);

	m_directIlluminationRTPassResult->createView(
		VK_IMAGE_VIEW_TYPE_2D,
		hri::ImageResource::DefaultComponentMapping(),
		hri::ImageResource::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1)
	);

	// Update descriptor sets
	for (size_t frameIdx = 0; frameIdx < hri::RenderCore::framesInFlight(); frameIdx++)
	{
		RendererFrameData& rendererFrameData = m_frames[frameIdx];
		updateFrameDescriptors(rendererFrameData);
	}
}

void Renderer::updateFrameDescriptors(RendererFrameData& frame)
{
	// Scene data set
	{
		VkDescriptorBufferInfo cameraUBOInfo = VkDescriptorBufferInfo{};
		cameraUBOInfo.buffer = frame.cameraUBO->buffer;
		cameraUBOInfo.offset = 0;
		cameraUBOInfo.range = frame.cameraUBO->bufferSize;

		VkDescriptorBufferInfo instanceSSBOInfo = VkDescriptorBufferInfo{};
		instanceSSBOInfo.buffer = m_activeScene.buffers.instanceDataSSBO.buffer;
		instanceSSBOInfo.offset = 0;
		instanceSSBOInfo.range = m_activeScene.buffers.instanceDataSSBO.bufferSize;

		VkDescriptorBufferInfo materialSSBOInfo = VkDescriptorBufferInfo{};
		materialSSBOInfo.buffer = m_activeScene.buffers.materialSSBO.buffer;
		materialSSBOInfo.offset = 0;
		materialSSBOInfo.range = m_activeScene.buffers.materialSSBO.bufferSize;

		(*frame.sceneDataSet)
			.writeBuffer(SceneDataBindings::Camera, &cameraUBOInfo)
			.writeBuffer(SceneDataBindings::RenderInstanceData, &instanceSSBOInfo)
			.writeBuffer(SceneDataBindings::MaterialData, &materialSSBOInfo)
			.flush();
	}

	// Ray tracing set
	{
		VkDescriptorImageInfo gbufferAlbedoResult = VkDescriptorImageInfo{};
		gbufferAlbedoResult.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		gbufferAlbedoResult.imageView = m_gbufferLayoutPassManager->getAttachmentResource(0).view;
		gbufferAlbedoResult.sampler = m_renderResultNearestSampler->sampler;

		VkDescriptorImageInfo gbufferWorldPosResult = VkDescriptorImageInfo{};
		gbufferWorldPosResult.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		gbufferWorldPosResult.imageView = m_gbufferLayoutPassManager->getAttachmentResource(1).view;
		gbufferWorldPosResult.sampler = m_renderResultNearestSampler->sampler;

		VkDescriptorImageInfo gbufferNormalResult = VkDescriptorImageInfo{};
		gbufferNormalResult.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		gbufferNormalResult.imageView = m_gbufferLayoutPassManager->getAttachmentResource(2).view;
		gbufferNormalResult.sampler = m_renderResultNearestSampler->sampler;

		VkDescriptorImageInfo softShadowOutInfo = VkDescriptorImageInfo{};
		softShadowOutInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		softShadowOutInfo.imageView = m_softShadowRTPassResult->view;
		softShadowOutInfo.sampler = VK_NULL_HANDLE;

		VkDescriptorImageInfo DIOutInfo = VkDescriptorImageInfo{};
		DIOutInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		DIOutInfo.imageView = m_directIlluminationRTPassResult->view;
		DIOutInfo.sampler = VK_NULL_HANDLE;

		(*frame.raytracingSet)
			.writeImage(RayTracingBindings::GBufferAlbedo, &gbufferAlbedoResult)
			.writeImage(RayTracingBindings::GBufferWorldPos, &gbufferWorldPosResult)
			.writeImage(RayTracingBindings::GBufferNormal, &gbufferNormalResult)
			.writeImage(RayTracingBindings::SoftShadowOutImage, &softShadowOutInfo)
			.writeImage(RayTracingBindings::DIOutImage, &DIOutInfo)
			.flush();
	}

	// Present input set
	{
		VkDescriptorImageInfo renderResultInfo = VkDescriptorImageInfo{};
		renderResultInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		renderResultInfo.imageView = m_directIlluminationRTPassResult->view;
		renderResultInfo.sampler = m_renderResultLinearSampler->sampler;

		(*frame.presentInputSet)
			.writeImage(PresentInputBindings::RenderResult, &renderResultInfo)
			.flush();
	}
}

void Renderer::prepareFrameResources(uint32_t frameIdx)
{
	assert(frameIdx < hri::RenderCore::framesInFlight());

	const std::vector<RenderInstance>& renderInstanceList = m_activeScene.generateRenderInstanceList(m_camera);
	RendererFrameData& rendererFrameData = m_frames[frameIdx];

	// Update UBO staging buffers
	hri::CameraShaderData cameraData = m_camera.getShaderData();
	rendererFrameData.cameraUBO->copyToBuffer(&cameraData, sizeof(hri::CameraShaderData));

	// Rebuild BLASses & TLAS
	{
		if (rendererFrameData.tlas.get() == nullptr)
		{
			rendererFrameData.tlas = std::make_unique<raytracing::AccelerationStructure>(
				m_activeScene.accelStructManager.createTLAS(renderInstanceList, rendererFrameData.blasList)
			);
		}

		if (m_activeScene.accelStructManager.shouldReallocTLAS(*rendererFrameData.tlas, renderInstanceList, rendererFrameData.blasList))
		{
			rendererFrameData.tlas = std::make_unique<raytracing::AccelerationStructure>(
				m_activeScene.accelStructManager.createTLAS(renderInstanceList, rendererFrameData.blasList)
			);
		}

		VkCommandBuffer oneshot = m_computePool.createCommandBuffer();
		m_activeScene.accelStructManager.cmdBuildBLASses(oneshot, m_activeScene.meshes, rendererFrameData.blasList);
		m_activeScene.accelStructManager.cmdBuildTLAS(oneshot, renderInstanceList, rendererFrameData.blasList, *rendererFrameData.tlas);
		m_computePool.submitAndWait(oneshot);
		m_computePool.freeCommandBuffer(oneshot);
	}

	// Always write TLAS descriptor in case of updated TLAS set
	VkWriteDescriptorSetAccelerationStructureKHR tlasInfo = VkWriteDescriptorSetAccelerationStructureKHR{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR };
	tlasInfo.accelerationStructureCount = 1;
	tlasInfo.pAccelerationStructures = &rendererFrameData.tlas->accelerationStructure;

	(*rendererFrameData.raytracingSet)
		.writeEXT(RayTracingBindings::Tlas, &tlasInfo)
		.flush();

	// Update subsystem frame info
	m_gbufferLayoutSubsystem->updateFrameInfo(GBufferLayoutFrameInfo{
		rendererFrameData.sceneDataSet->set,
		&m_activeScene,
	});

	m_hybridRTSubsystem->updateFrameInfo(RayTracingFrameInfo{
		rendererFrameData.sceneDataSet->set,
		rendererFrameData.raytracingSet->set,
	});

	m_presentSubsystem->updateFrameInfo(PresentFrameInfo{
		rendererFrameData.presentInputSet->set,
	});
}
