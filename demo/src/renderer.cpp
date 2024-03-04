#include "renderer.h"

#include <hybrid_renderer.h>
#include <memory>

#include "detail/raytracing.h"
#include "subsystems.h"

void RayTracingTargets::init(hri::RenderContext& ctx, RayTracingTargets& targets)
{
	VkExtent2D swapExtent = ctx.swapchain.extent;

	targets.globalIllumination = std::unique_ptr<hri::ImageResource>(new hri::ImageResource(
		ctx,
		VK_IMAGE_TYPE_2D,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_SAMPLE_COUNT_1_BIT,
		{ swapExtent.width, swapExtent.height, 1 },
		1,
		1,
		VK_IMAGE_USAGE_STORAGE_BIT
		| VK_IMAGE_USAGE_SAMPLED_BIT
	));

	targets.globalIllumination->createView(
		VK_IMAGE_VIEW_TYPE_2D,
		hri::ImageResource::DefaultComponentMapping(),
		hri::ImageResource::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1)
	);
}

Renderer::Renderer(raytracing::RayTracingContext& ctx, hri::Camera& camera, SceneGraph& activeScene)
	:
	m_context(ctx.renderContext),
	m_raytracingContext(ctx),
	m_debug(m_context),
	m_renderCore(m_context),
	m_shaderDatabase(m_context),
	m_descriptorSetAllocator(m_context),
	m_accelStructManager(ctx),
	m_computePool(m_context, m_context.queues.computeQueue, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT),
	m_stagingPool(m_context, m_context.queues.transferQueue, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT),
	m_frameCounter(0),
	m_prevCamera(camera),
	m_camera(camera),
	m_activeScene(activeScene)
{
	assert(m_activeScene.lightCount > 0);

	initShaderDB();
	initRenderPasses();
	initSharedResources();
	initGlobalDescriptorSets();
	initRenderSubsystems();
	initRendererFrameData();

	m_renderCore.setOnSwapchainInvalidateCallback([&](const vkb::Swapchain& swapchain) {
		recreateSwapDependentResources(swapchain);
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
	recreateSwapDependentResources(m_context.swapchain);
}

void Renderer::drawFrame()
{
	m_renderCore.startFrame();
	hri::ActiveFrame frame = m_renderCore.getActiveFrame();
	RendererFrameData& frameData = m_frames[frame.currentFrameIndex];

	// Prepare next frame's resources
	uint32_t nextFrameIdx = (frame.currentFrameIndex + 1) % hri::RenderCore::framesInFlight();
	prepareFrameResources(nextFrameIdx);

	// Begin command recording for this frame
	frame.beginCommands();

	// Record GBufferLayout pass
	m_gbufferLayoutPassManager->beginRenderPass(frame);
	m_gbufferLayoutSubsystem->record(frame, GBufferLayoutFrameInfo{ frameData.sceneDataSet->set, &m_activeScene, });
	m_gbufferLayoutPassManager->endRenderPass(frame);

	// Execute raytracing passes
	RayTracingFrameInfo rtFrameInfo = RayTracingFrameInfo{
		m_frameCounter,
		&m_gbufferLayoutPassManager->getAttachmentResource(0),
		&m_gbufferLayoutPassManager->getAttachmentResource(1),
		&m_gbufferLayoutPassManager->getAttachmentResource(2),
		&m_gbufferLayoutPassManager->getAttachmentResource(3),
		&m_gbufferLayoutPassManager->getAttachmentResource(4),
		&m_gbufferLayoutPassManager->getAttachmentResource(5),
		m_raytracingTargets.globalIllumination.get(),
		frameData.sceneDataSet->set,
		frameData.raytracingSet->set
	};

	m_GISubsystem->transitionGbufferResources(frame, rtFrameInfo);
	m_GISubsystem->record(frame, rtFrameInfo);

	// Transition raytracing resources
	{
		VkImageMemoryBarrier2 sampleBarrier = VkImageMemoryBarrier2{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
		sampleBarrier.srcStageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
		sampleBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
		sampleBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT_KHR;
		sampleBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
		sampleBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		sampleBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		sampleBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		sampleBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

		sampleBarrier.image = m_raytracingTargets.globalIllumination->image;
		sampleBarrier.subresourceRange = hri::ImageResource::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);
		frame.pipelineBarrier({ sampleBarrier });
	}

	// Record a deferred shading pass, composing the final output image
	m_deferredShadingPassManager->beginRenderPass(frame);
	m_deferredShadingSubsystem->record(frame, DeferredShadingFrameInfo{
		frameData.deferredShadingSet->set
	});
	m_deferredShadingPassManager->endRenderPass(frame);

	// Transition presentable image
	{
		VkImageMemoryBarrier2 presentSampleBarrier = VkImageMemoryBarrier2{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
		presentSampleBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		presentSampleBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
		presentSampleBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		presentSampleBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
		presentSampleBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		presentSampleBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		presentSampleBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		presentSampleBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		presentSampleBarrier.image = m_deferredShadingPassManager->getAttachmentResource(0).image;
		presentSampleBarrier.subresourceRange = hri::ImageResource::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);

		frame.pipelineBarrier({ presentSampleBarrier });
	}

	// Record swapchain output passes
	m_swapchainPassManager->beginRenderPass(frame);
	m_presentSubsystem->record(frame, PresentFrameInfo{
		frameData.presentInputSet->set
	});
	m_uiSubsystem->record(frame, UIFrameInfo{});
	m_swapchainPassManager->endRenderPass(frame);

	frame.endCommands();
	m_renderCore.endFrame();

	m_prevCamera = m_camera;
	m_frameCounter++;
}

void Renderer::initShaderDB()
{
	// Raytracing Shaders
	m_shaderDatabase.registerShader("GIRayGen", hri::Shader::loadFile(m_context, "./shaders/gi_hybrid.rgen.spv", VK_SHADER_STAGE_RAYGEN_BIT_KHR));
	m_shaderDatabase.registerShader("GIMiss", hri::Shader::loadFile(m_context, "./shaders/gi.rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR));
	m_shaderDatabase.registerShader("GICHit", hri::Shader::loadFile(m_context, "./shaders/gi.rchit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR));

	// Vertex shaders
	m_shaderDatabase.registerShader("PresentVert", hri::Shader::loadFile(m_context, "./shaders/present.vert.spv", VK_SHADER_STAGE_VERTEX_BIT));
	m_shaderDatabase.registerShader("StaticVert", hri::Shader::loadFile(m_context, "./shaders/static.vert.spv", VK_SHADER_STAGE_VERTEX_BIT));

	// Fragment shaders
	m_shaderDatabase.registerShader("GBufferLayoutFrag", hri::Shader::loadFile(m_context, "./shaders/gbuffer_layout.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT));
	m_shaderDatabase.registerShader("DeferredShadingFrag", hri::Shader::loadFile(m_context, "./shaders/deferred_shading.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT));
	m_shaderDatabase.registerShader("PresentFrag", hri::Shader::loadFile(m_context, "./shaders/present.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT));
}

void Renderer::initRenderPasses()
{
	// GBuffer Layout pass
	{
		hri::RenderPassBuilder gbufferLayoutPassBuilder = hri::RenderPassBuilder(m_context)
			.addDependency(
				VK_SUBPASS_EXTERNAL,
				0,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
			)
			.addAttachment(	// Albedo
				VK_FORMAT_R8G8B8A8_UNORM,
				VK_SAMPLE_COUNT_1_BIT,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_ATTACHMENT_LOAD_OP_CLEAR,
				VK_ATTACHMENT_STORE_OP_STORE
			)
			.addAttachment(	// Emission
				VK_FORMAT_R32G32B32A32_SFLOAT,
				VK_SAMPLE_COUNT_1_BIT,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_ATTACHMENT_LOAD_OP_CLEAR,
				VK_ATTACHMENT_STORE_OP_STORE
			)
			.addAttachment( // Specular & Shininess
				VK_FORMAT_R32G32B32A32_SFLOAT,
				VK_SAMPLE_COUNT_1_BIT,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_ATTACHMENT_LOAD_OP_CLEAR,
				VK_ATTACHMENT_STORE_OP_STORE
			)
			.addAttachment( // Transmittance & IOR
				VK_FORMAT_R32G32B32A32_SFLOAT,
				VK_SAMPLE_COUNT_1_BIT,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_ATTACHMENT_LOAD_OP_CLEAR,
				VK_ATTACHMENT_STORE_OP_STORE
			)
			.addAttachment(	// Normal
				VK_FORMAT_R32G32B32A32_SFLOAT,
				VK_SAMPLE_COUNT_1_BIT,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_ATTACHMENT_LOAD_OP_CLEAR,
				VK_ATTACHMENT_STORE_OP_STORE
			)
			.addAttachment(	// Depth
				VK_FORMAT_D32_SFLOAT,
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
				hri::AttachmentType::Color,
				VkAttachmentReference{ 3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }
			)
			.setAttachmentReference(
				hri::AttachmentType::Color,
				VkAttachmentReference{ 4, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }
			)
			.setAttachmentReference(
				hri::AttachmentType::DepthStencil,
				VkAttachmentReference{ 5, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL }
			);

		const std::vector<hri::RenderAttachmentConfig> gbufferAttachmentConfigs = {
			hri::RenderAttachmentConfig{ // Albedo target
				VK_FORMAT_R8G8B8A8_UNORM,
				VK_SAMPLE_COUNT_1_BIT,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
				| VK_IMAGE_USAGE_SAMPLED_BIT,
				VK_IMAGE_ASPECT_COLOR_BIT
			},
			hri::RenderAttachmentConfig{ // Emission target
				VK_FORMAT_R32G32B32A32_SFLOAT,
				VK_SAMPLE_COUNT_1_BIT,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
				| VK_IMAGE_USAGE_SAMPLED_BIT,
				VK_IMAGE_ASPECT_COLOR_BIT
			},
			hri::RenderAttachmentConfig{ // Material specular & shininess target
				VK_FORMAT_R32G32B32A32_SFLOAT,
				VK_SAMPLE_COUNT_1_BIT,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
				| VK_IMAGE_USAGE_SAMPLED_BIT,
				VK_IMAGE_ASPECT_COLOR_BIT
			},
			hri::RenderAttachmentConfig{ // Material transmittance & IOR target
				VK_FORMAT_R32G32B32A32_SFLOAT,
				VK_SAMPLE_COUNT_1_BIT,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
				| VK_IMAGE_USAGE_SAMPLED_BIT,
				VK_IMAGE_ASPECT_COLOR_BIT
			},
			hri::RenderAttachmentConfig{ // Normal target
				VK_FORMAT_R32G32B32A32_SFLOAT,
				VK_SAMPLE_COUNT_1_BIT,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
				| VK_IMAGE_USAGE_SAMPLED_BIT,
				VK_IMAGE_ASPECT_COLOR_BIT
			},
			hri::RenderAttachmentConfig{ // Depth Stencil target
				VK_FORMAT_D32_SFLOAT,
				VK_SAMPLE_COUNT_1_BIT,
				VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
				| VK_IMAGE_USAGE_SAMPLED_BIT,
				VK_IMAGE_ASPECT_DEPTH_BIT
			},
		};

		m_gbufferLayoutPassManager = std::unique_ptr<hri::RenderPassResourceManager>(new hri::RenderPassResourceManager(
			m_context,
			gbufferLayoutPassBuilder.build(),
			gbufferAttachmentConfigs
		));
	}

	// deferred pass
	{
		hri::RenderPassBuilder deferredPassBuilder = hri::RenderPassBuilder(m_context)
			.addAttachment(
				VK_FORMAT_R8G8B8A8_UNORM,
				VK_SAMPLE_COUNT_1_BIT,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_ATTACHMENT_LOAD_OP_CLEAR,
				VK_ATTACHMENT_STORE_OP_STORE
			)
			.setAttachmentReference(
				hri::AttachmentType::Color,
				VkAttachmentReference{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }
			);

		const std::vector<hri::RenderAttachmentConfig> deferredAttachmentConfigs = {
			hri::RenderAttachmentConfig{
				VK_FORMAT_R8G8B8A8_UNORM,
				VK_SAMPLE_COUNT_1_BIT,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
				| VK_IMAGE_USAGE_SAMPLED_BIT,
				VK_IMAGE_ASPECT_COLOR_BIT
			},
		};

		m_deferredShadingPassManager = std::unique_ptr<hri::RenderPassResourceManager>(new hri::RenderPassResourceManager(
			m_context,
			deferredPassBuilder.build(),
			deferredAttachmentConfigs
		));
	}

	// Swapchain pass
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
	m_gbufferLayoutPassManager->setClearValue(3, VkClearValue{ { 0.0f, 0.0f, 0.0f, 0.0f } });
	m_gbufferLayoutPassManager->setClearValue(4, VkClearValue{ { 0.0f, 0.0f, 0.0f, 0.0f } });
	m_gbufferLayoutPassManager->setClearValue(5, VkClearValue{ { 1.0f, 0x00 } });

	m_deferredShadingPassManager->setClearValue(0, VkClearValue{ { 0.0f, 0.0f, 0.0f, 0.0f } });

	m_swapchainPassManager->setClearValue(0, VkClearValue{ { 0.0f, 0.0f, 0.0f, 1.0f } });
}

void Renderer::initSharedResources()
{
	VkExtent2D swapExtent = m_context.swapchain.extent;

	// init shared samplers
	{
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
	}

	// Init ray tracing targets
	RayTracingTargets::init(m_context, m_raytracingTargets);
}

void Renderer::initGlobalDescriptorSets()
{
	// Create set layouts
	hri::DescriptorSetLayoutBuilder sceneDataSetBuilder = hri::DescriptorSetLayoutBuilder(m_context)
		.addBinding(
			SceneDataBindings::Camera,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			VK_SHADER_STAGE_VERTEX_BIT
			| VK_SHADER_STAGE_RAYGEN_BIT_KHR
		)
		.addBinding(
			SceneDataBindings::RenderInstanceBuffer,
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			VK_SHADER_STAGE_VERTEX_BIT
			| VK_SHADER_STAGE_FRAGMENT_BIT
			| VK_SHADER_STAGE_RAYGEN_BIT_KHR
			| VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
			| VK_SHADER_STAGE_ANY_HIT_BIT_KHR
			| VK_SHADER_STAGE_MISS_BIT_KHR
		)
		.addBinding(
			SceneDataBindings::MaterialBuffer, 
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			VK_SHADER_STAGE_VERTEX_BIT
			| VK_SHADER_STAGE_FRAGMENT_BIT
			| VK_SHADER_STAGE_RAYGEN_BIT_KHR
			| VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
			| VK_SHADER_STAGE_ANY_HIT_BIT_KHR
			| VK_SHADER_STAGE_MISS_BIT_KHR
		)
		.addBinding(
			SceneDataBindings::LightBuffer,
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			VK_SHADER_STAGE_RAYGEN_BIT_KHR
		);

	hri::DescriptorSetLayoutBuilder rtGlobalDescriptorSetBuilder = hri::DescriptorSetLayoutBuilder(m_context)
		.addBinding(RayTracingBindings::Tlas, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
		.addBinding(RayTracingBindings::GBufferAlbedo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.addBinding(RayTracingBindings::GBufferEmission, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.addBinding(RayTracingBindings::GBufferMatSpecular, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.addBinding(RayTracingBindings::GBufferMatTransmittance, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.addBinding(RayTracingBindings::GBufferNormal, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.addBinding(RayTracingBindings::GBufferDepth, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.addBinding(RayTracingBindings::GIOutImage, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR);

	hri::DescriptorSetLayoutBuilder deferredShadingSetBuilder = hri::DescriptorSetLayoutBuilder(m_context)
		.addBinding(DeferredShadingBindings::GlobalIlluminationResult, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

	hri::DescriptorSetLayoutBuilder presentInputSetBuilder = hri::DescriptorSetLayoutBuilder(m_context)
		.addBinding(PresentInputBindings::RenderResult, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

	// Allocate descriptor set layouts
	m_sceneDataSetLayout = std::unique_ptr<hri::DescriptorSetLayout>(new hri::DescriptorSetLayout(sceneDataSetBuilder.build()));
	m_rtDescriptorSetLayout = std::unique_ptr<hri::DescriptorSetLayout>(new hri::DescriptorSetLayout(rtGlobalDescriptorSetBuilder.build()));
	m_deferredShadingSetLayout = std::unique_ptr<hri::DescriptorSetLayout>(new hri::DescriptorSetLayout(deferredShadingSetBuilder.build()));
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

	m_GISubsystem = std::unique_ptr<GlobalIlluminationSubsystem>(new GlobalIlluminationSubsystem(
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

	m_deferredShadingSubsystem = std::unique_ptr<DeferredShadingSubsystem>(new DeferredShadingSubsystem(
		m_context,
		m_shaderDatabase,
		m_deferredShadingPassManager->renderPass(),
		m_deferredShadingSetLayout->setLayout
	));

	m_presentSubsystem = std::unique_ptr<PresentationSubsystem>(new PresentationSubsystem(
		m_context,
		m_shaderDatabase,
		m_swapchainPassManager->renderPass(),
		m_presentInputSetLayout->setLayout
	));
}

void Renderer::initRendererFrameData()
{
	for (size_t i = 0; i < hri::RenderCore::framesInFlight(); i++)
	{
		RendererFrameData& frame = m_frames[i];
		frame.cameraUBO = std::unique_ptr<hri::BufferResource>(new hri::BufferResource(
			m_context, sizeof(hri::CameraShaderData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, true
		));

		frame.lightSSBO = std::unique_ptr<hri::BufferResource>(new hri::BufferResource(
			m_context, sizeof(LightArrayEntry) * m_activeScene.lightCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true
		));

		frame.blasList = m_accelStructManager.createBLASList(m_activeScene.meshes);

		frame.sceneDataSet = std::unique_ptr<hri::DescriptorSetManager>(new hri::DescriptorSetManager(
			m_context, m_descriptorSetAllocator, *m_sceneDataSetLayout
		));
		frame.raytracingSet = std::unique_ptr<hri::DescriptorSetManager>(new hri::DescriptorSetManager(
			m_context, m_descriptorSetAllocator, *m_rtDescriptorSetLayout
		));
		frame.deferredShadingSet = std::unique_ptr<hri::DescriptorSetManager>(new hri::DescriptorSetManager(
			m_context, m_descriptorSetAllocator, *m_deferredShadingSetLayout
		));
		frame.presentInputSet = std::unique_ptr<hri::DescriptorSetManager>(new hri::DescriptorSetManager(
			m_context, m_descriptorSetAllocator, *m_presentInputSetLayout
		));

		updateFrameDescriptors(frame);
	}
}

void Renderer::recreateSwapDependentResources(const vkb::Swapchain& swapchain)
{
	VkExtent2D swapExtent = swapchain.extent;

	// Recreate render pass resources
	m_gbufferLayoutPassManager->recreateResources();
	m_deferredShadingPassManager->recreateResources();
	m_swapchainPassManager->recreateResources();

	// Reinit raytracing targets
	RayTracingTargets::init(m_context, m_raytracingTargets);

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

		VkDescriptorBufferInfo lightSSBOInfo = VkDescriptorBufferInfo{};
		lightSSBOInfo.buffer = frame.lightSSBO->buffer;
		lightSSBOInfo.offset = 0;
		lightSSBOInfo.range = frame.lightSSBO->bufferSize;

		(*frame.sceneDataSet)
			.writeBuffer(SceneDataBindings::Camera, &cameraUBOInfo)
			.writeBuffer(SceneDataBindings::RenderInstanceBuffer, &instanceSSBOInfo)
			.writeBuffer(SceneDataBindings::MaterialBuffer, &materialSSBOInfo)
			.writeBuffer(SceneDataBindings::LightBuffer, &lightSSBOInfo)
			.flush();
	}

	// Ray tracing set
	{
		VkDescriptorImageInfo gbufferAlbedoResult = VkDescriptorImageInfo{};
		gbufferAlbedoResult.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		gbufferAlbedoResult.imageView = m_gbufferLayoutPassManager->getAttachmentResource(0).view;
		gbufferAlbedoResult.sampler = m_renderResultNearestSampler->sampler;

		VkDescriptorImageInfo gbufferEmissionResult = VkDescriptorImageInfo{};
		gbufferEmissionResult.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		gbufferEmissionResult.imageView = m_gbufferLayoutPassManager->getAttachmentResource(1).view;
		gbufferEmissionResult.sampler = m_renderResultNearestSampler->sampler;
		
		VkDescriptorImageInfo gbufferSpecularResult = VkDescriptorImageInfo{};
		gbufferSpecularResult.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		gbufferSpecularResult.imageView = m_gbufferLayoutPassManager->getAttachmentResource(2).view;
		gbufferSpecularResult.sampler = m_renderResultNearestSampler->sampler;

		VkDescriptorImageInfo gbufferTransmittanceResult = VkDescriptorImageInfo{};
		gbufferTransmittanceResult.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		gbufferTransmittanceResult.imageView = m_gbufferLayoutPassManager->getAttachmentResource(2).view;
		gbufferTransmittanceResult.sampler = m_renderResultNearestSampler->sampler;

		VkDescriptorImageInfo gbufferNormalResult = VkDescriptorImageInfo{};
		gbufferNormalResult.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		gbufferNormalResult.imageView = m_gbufferLayoutPassManager->getAttachmentResource(4).view;
		gbufferNormalResult.sampler = m_renderResultNearestSampler->sampler;

		VkDescriptorImageInfo gbufferDepthResult = VkDescriptorImageInfo{};
		gbufferDepthResult.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		gbufferDepthResult.imageView = m_gbufferLayoutPassManager->getAttachmentResource(5).view;
		gbufferDepthResult.sampler = m_renderResultNearestSampler->sampler;

		VkDescriptorImageInfo GIOutInfo = VkDescriptorImageInfo{};
		GIOutInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		GIOutInfo.imageView = m_raytracingTargets.globalIllumination->view;
		GIOutInfo.sampler = VK_NULL_HANDLE;

		(*frame.raytracingSet)
			.writeImage(RayTracingBindings::GBufferAlbedo, &gbufferAlbedoResult)
			.writeImage(RayTracingBindings::GBufferEmission, &gbufferEmissionResult)
			.writeImage(RayTracingBindings::GBufferMatSpecular, &gbufferSpecularResult)
			.writeImage(RayTracingBindings::GBufferMatTransmittance, &gbufferTransmittanceResult)
			.writeImage(RayTracingBindings::GBufferNormal, &gbufferNormalResult)
			.writeImage(RayTracingBindings::GBufferDepth, &gbufferDepthResult)
			.writeImage(RayTracingBindings::GIOutImage, &GIOutInfo)
			.flush();
	}

	// Deferred shading set
	{
		VkDescriptorImageInfo giResult = VkDescriptorImageInfo{};
		giResult.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		giResult.imageView = m_raytracingTargets.globalIllumination->view;
		giResult.sampler = m_renderResultLinearSampler->sampler;

		(*frame.deferredShadingSet)
			.writeImage(DeferredShadingBindings::GlobalIlluminationResult, &giResult)
			.flush();
	}

	// Present input set
	{
		VkDescriptorImageInfo renderResultInfo = VkDescriptorImageInfo{};
		renderResultInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		renderResultInfo.imageView = m_deferredShadingPassManager->getAttachmentResource(0).view;
		renderResultInfo.sampler = m_renderResultLinearSampler->sampler;

		(*frame.presentInputSet)
			.writeImage(PresentInputBindings::RenderResult, &renderResultInfo)
			.flush();
	}
}

void Renderer::prepareFrameResources(uint32_t frameIdx)
{
	assert(frameIdx < hri::RenderCore::framesInFlight());
	m_renderCore.awaitFrameFinished(frameIdx);	// Ensure this frame's execution has finished

	const std::vector<RenderInstance>& renderInstanceList = m_activeScene.generateRenderInstanceList(m_camera);
	RendererFrameData& rendererFrameData = m_frames[frameIdx];

	// Update UBO staging buffers
	hri::CameraShaderData cameraData = m_camera.getShaderData();
	rendererFrameData.cameraUBO->copyToBuffer(&cameraData, sizeof(hri::CameraShaderData));

	// Fill instance light buffer
	std::vector<LightArrayEntry> lightBuffer; lightBuffer.reserve(m_activeScene.lightCount);
	for (auto const& instance : renderInstanceList)
	{
		const RenderInstanceData& instanceData = m_activeScene.getInstanceData(instance.instanceId);
		const Material& mat = m_activeScene.materials[instanceData.materialIdx];
		if (mat.emission.r > 0.0 || mat.emission.g > 0.0 || mat.emission.b > 0.0)
		{
			lightBuffer.push_back(LightArrayEntry{
				instance.instanceId
			});
		}
	}
	assert(lightBuffer.size() == m_activeScene.lightCount);
	rendererFrameData.lightSSBO->copyToBuffer(lightBuffer.data(), rendererFrameData.lightSSBO->bufferSize);

	// Create or reallocate TLAS
	if (rendererFrameData.tlas.get() == nullptr
		|| m_accelStructManager.shouldReallocTLAS(*rendererFrameData.tlas, renderInstanceList, rendererFrameData.blasList)
	)
	{
		rendererFrameData.tlas = std::make_unique<raytracing::AccelerationStructure>(
			m_accelStructManager.createTLAS(renderInstanceList, rendererFrameData.blasList)
		);
	}

	// Build BLAS list & TLAS
	VkCommandBuffer oneshot = m_computePool.createCommandBuffer();
	m_accelStructManager.cmdBuildBLASses(oneshot, renderInstanceList, m_activeScene.meshes, rendererFrameData.blasList);
	m_accelStructManager.cmdBuildTLAS(oneshot, renderInstanceList, rendererFrameData.blasList, *rendererFrameData.tlas);
	m_computePool.submitAndWait(oneshot);
	m_computePool.freeCommandBuffer(oneshot);

	// Always write TLAS descriptor in case of updated TLAS structure
	VkWriteDescriptorSetAccelerationStructureKHR tlasInfo = VkWriteDescriptorSetAccelerationStructureKHR{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR };
	tlasInfo.accelerationStructureCount = 1;
	tlasInfo.pAccelerationStructures = &rendererFrameData.tlas->accelerationStructure;

	(*rendererFrameData.raytracingSet)
		.writeEXT(RayTracingBindings::Tlas, &tlasInfo)
		.flush();
}
