#include "renderer.h"

#include <hybrid_renderer.h>
#include <memory>

#include "detail/raytracing.h"
#include "render_passes.h"

#define SHOW_DEBUG_OUTPUT			1

Renderer::Renderer(raytracing::RayTracingContext& ctx, hri::Camera& camera, SceneGraph& activeScene)
	:
	m_context(ctx.renderContext),
	m_raytracingContext(ctx),
	m_renderCore(m_context),
	m_shaderDatabase(m_context),
	m_descriptorSetAllocator(m_context),
	m_computePool(m_context, m_context.queues.computeQueue, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT),
	m_stagingPool(m_context, m_context.queues.transferQueue, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT),
	m_asBuildTimer(m_context),
	m_accelerationStructureManager(ctx),
	m_frameCounter(1),
	m_subFrameCounter(1),
	m_prevCamera(camera),
	m_camera(camera),
	m_activeScene(activeScene)
{
	assert(m_activeScene.lightCount > 0);

	// Set up frame resources
	auto instances = m_activeScene.generateRenderInstanceList(m_camera);
	m_frameResources = CommonResources{};
	m_frameResources.prevCameraUBO = std::unique_ptr<hri::BufferResource>(new hri::BufferResource(m_context, sizeof(hri::CameraShaderData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, true));
	m_frameResources.cameraUBO = std::unique_ptr<hri::BufferResource>(new hri::BufferResource(m_context, sizeof(hri::CameraShaderData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, true));
	m_frameResources.instanceDataSSBO = &m_activeScene.buffers.instanceDataSSBO;	// FIXME: accessing scene buffers like this is ugly, manage in renderer maybe?
	m_frameResources.materialSSBO = &m_activeScene.buffers.materialSSBO;			// Same here, also ugly
	m_frameResources.blasList = m_accelerationStructureManager.createBLASList(m_activeScene.meshes);
	m_frameResources.tlas = std::make_unique<raytracing::AccelerationStructure>(m_accelerationStructureManager.createTLAS(instances, m_frameResources.blasList));

	initRenderPasses();
	m_renderCore.setOnSwapchainInvalidateCallback([&](const vkb::Swapchain& swapchain) { recreateSwapDependentResources(swapchain);	});
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

void Renderer::resetAccumulators()
{
	m_renderCore.awaitFrameFinished();
	m_pathTracingPass->recreateResources(m_context.swapchain.extent);

	m_subFrameCounter = 1;
	prepareFrame();	// prepare frame resources again
}

void Renderer::prepareFrame()
{
	m_renderCore.awaitFrameFinished();

	// update instance list & frame resource state
	auto instances = m_activeScene.generateRenderInstanceList(m_camera);
	m_frameResources.frameIndex = m_frameCounter;
	m_frameResources.subFrameIndex = m_subFrameCounter;
	m_frameResources.activeScene = &m_activeScene;

	// Copy SSBO & UBO data to buffers and check if TLAS realloc is needed
	m_frameResources.prevCameraUBO->copyToBuffer(&m_prevCamera.getShaderData(), sizeof(hri::CameraShaderData));
	m_frameResources.cameraUBO->copyToBuffer(&m_camera.getShaderData(), sizeof(hri::CameraShaderData));
	if (m_accelerationStructureManager.shouldReallocTLAS(*m_frameResources.tlas, instances, m_frameResources.blasList))
		m_frameResources.tlas = std::make_unique<raytracing::AccelerationStructure>(m_accelerationStructureManager.createTLAS(instances, m_frameResources.blasList));

	// Build acceleration structures for this frame
	VkCommandBuffer ASBuildCommands = m_computePool.createCommandBuffer(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	m_asBuildTimer.resetTimer();
	m_asBuildTimer.cmdBeginLabel(ASBuildCommands, "Acceleration Structure Rebuild");
	m_asBuildTimer.cmdRecordStartTimestamp(ASBuildCommands);
	m_accelerationStructureManager.cmdBuildBLASses(ASBuildCommands, instances, m_activeScene.meshes, m_frameResources.blasList);
	m_accelerationStructureManager.cmdBuildTLAS(ASBuildCommands, instances, m_frameResources.blasList, *m_frameResources.tlas);
	m_asBuildTimer.cmdRecordEndTimestamp(ASBuildCommands);
	m_asBuildTimer.cmdEndLabel(ASBuildCommands);

	m_computePool.submitAndWait(ASBuildCommands);
	m_computePool.freeCommandBuffer(ASBuildCommands);

	// Prepare pass I/O descriptors
	VkDescriptorImageInfo renderResultInfo = VkDescriptorImageInfo{};
	if (usePathTracer)
	{
		renderResultInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		renderResultInfo.imageView = m_pathTracingPass->getRenderResultView();
		renderResultInfo.sampler = m_presentPass->passInputSampler->sampler;
	}
	else
	{
		// Set GBuffer sampling descriptors
		auto writeGBufferSampleDescriptors = [](
			hri::DescriptorSetManager& descriptorSet,
			hri::ImageSampler& sampler,
			hri::RenderPassResourceManager& manager
		) {
			VkDescriptorImageInfo albedoInfo = VkDescriptorImageInfo{ sampler.sampler, manager.getAttachmentResource(0).view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
			VkDescriptorImageInfo emissionInfo = VkDescriptorImageInfo{ sampler.sampler, manager.getAttachmentResource(1).view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
			VkDescriptorImageInfo specularInfo = VkDescriptorImageInfo{ sampler.sampler, manager.getAttachmentResource(2).view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
			VkDescriptorImageInfo transmittanceInfo = VkDescriptorImageInfo{ sampler.sampler, manager.getAttachmentResource(3).view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
			VkDescriptorImageInfo normalInfo = VkDescriptorImageInfo{ sampler.sampler, manager.getAttachmentResource(4).view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
			VkDescriptorImageInfo maskInfo = VkDescriptorImageInfo{ sampler.sampler, manager.getAttachmentResource(5).view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
			VkDescriptorImageInfo depthInfo = VkDescriptorImageInfo{ sampler.sampler, manager.getAttachmentResource(6).view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

			descriptorSet
				.writeImage(0, &albedoInfo)
				.writeImage(1, &emissionInfo)
				.writeImage(2, &specularInfo)
				.writeImage(3, &transmittanceInfo)
				.writeImage(4, &normalInfo)
				.writeImage(5, &maskInfo)
				.writeImage(6, &depthInfo)
				.flush();
		};

		VkDescriptorImageInfo rngSourceInfo = VkDescriptorImageInfo{};
		rngSourceInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		rngSourceInfo.imageView = m_rngGenPass->rngSource->view;
		rngSourceInfo.sampler = m_gbufferSamplePass->passInputSampler->sampler;
		(*m_gbufferSamplePass->rngDescriptorSet)
			.writeImage(0, &rngSourceInfo)
			.flush();

		writeGBufferSampleDescriptors(*m_gbufferSamplePass->loDefDescriptorSet, *m_gbufferSamplePass->passInputSampler, *m_gbufferLayoutPass->loDefLODPassResources);
		writeGBufferSampleDescriptors(*m_gbufferSamplePass->hiDefDescriptorSet, *m_gbufferSamplePass->passInputSampler, *m_gbufferLayoutPass->hiDefLODPassResources);

		// TODO: set direct illumination descriptors

		// Set Deferred shading descriptors
		VkDescriptorImageInfo deferredAlbedoInfo = VkDescriptorImageInfo{ m_deferredShadingPass->passInputSampler->sampler, m_gbufferSamplePass->passResources->getAttachmentResource(0).view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkDescriptorImageInfo deferredEmissionInfo = VkDescriptorImageInfo{ m_deferredShadingPass->passInputSampler->sampler, m_gbufferSamplePass->passResources->getAttachmentResource(1).view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkDescriptorImageInfo deferredSpecularInfo = VkDescriptorImageInfo{ m_deferredShadingPass->passInputSampler->sampler, m_gbufferSamplePass->passResources->getAttachmentResource(2).view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkDescriptorImageInfo deferredTransmittanceInfo = VkDescriptorImageInfo{ m_deferredShadingPass->passInputSampler->sampler, m_gbufferSamplePass->passResources->getAttachmentResource(3).view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkDescriptorImageInfo deferredNormalInfo = VkDescriptorImageInfo{ m_deferredShadingPass->passInputSampler->sampler, m_gbufferSamplePass->passResources->getAttachmentResource(4).view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		(*m_deferredShadingPass->inputDescriptorSet)
			.writeImage(0, &deferredAlbedoInfo)
			.writeImage(1, &deferredEmissionInfo)
			.writeImage(2, &deferredSpecularInfo)
			.writeImage(3, &deferredTransmittanceInfo)
			.writeImage(4, &deferredNormalInfo)
			.flush();

		// Set render result descriptors
		renderResultInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		renderResultInfo.imageView = m_deferredShadingPass->passResources->getAttachmentResource(0).view;
		renderResultInfo.sampler = m_presentPass->passInputSampler->sampler;
	}

	(*m_presentPass->presentDescriptorSet)
		.writeImage(0, &renderResultInfo)
		.flush();

	// Prepare per pass frame resources
	m_rngGenPass->prepareFrame(m_frameResources);
	m_pathTracingPass->prepareFrame(m_frameResources);
	m_gbufferLayoutPass->prepareFrame(m_frameResources);
	m_gbufferSamplePass->prepareFrame(m_frameResources);
	m_directIlluminationPass->prepareFrame(m_frameResources);
	m_deferredShadingPass->prepareFrame(m_frameResources);

	m_presentPass->prepareFrame(m_frameResources);
	m_uiPass->prepareFrame(m_frameResources);
}

void Renderer::drawFrame()
{
#if SHOW_DEBUG_OUTPUT == 1
	if (usePathTracer)
	{
		printf(
			"RNGGen: %8.4f ms, PathTracing: %8.4f ms, AS Build %8.4f ms\n",
			m_rngGenPass->debug.timeDelta(),
			m_pathTracingPass->debug.timeDelta(),
			m_asBuildTimer.timeDelta()
		);
	}
	else
	{
		printf(
			"RNGGen: %8.4f ms, GBufLayout: %8.4f ms, GBufSample: %8.4f ms, DI: %8.4f ms, DS: %8.4f ms, AS Build %8.4f ms\n",
			m_rngGenPass->debug.timeDelta(),
			m_gbufferLayoutPass->debug.timeDelta(),
			m_gbufferSamplePass->debug.timeDelta(),
			m_directIlluminationPass->debug.timeDelta(),
			m_deferredShadingPass->debug.timeDelta(),
			m_asBuildTimer.timeDelta()
		);
	}
#endif

	m_renderCore.startFrame();
	hri::ActiveFrame frame = m_renderCore.getActiveFrame();

	// Begin command recording for this frame
	frame.beginCommands();

	m_rngGenPass->drawFrame(frame, m_frameResources);

	if (usePathTracer)
	{
		m_pathTracingPass->drawFrame(frame, m_frameResources);
	}
	else
	{
		m_gbufferLayoutPass->drawFrame(frame, m_frameResources);
		m_gbufferSamplePass->drawFrame(frame, m_frameResources);
		m_directIlluminationPass->drawFrame(frame, m_frameResources);
		m_deferredShadingPass->drawFrame(frame, m_frameResources);
	}

	m_presentPass->drawFrame(frame, m_frameResources);
	m_uiPass->drawFrame(frame, m_frameResources);

	frame.endCommands();
	m_renderCore.endFrame();

	m_frameCounter += 1;
	m_subFrameCounter += 1;
	m_prevCamera = m_camera;
}

void Renderer::initRenderPasses()
{
	m_rngGenPass = std::unique_ptr<RngGenerationPass>(new RngGenerationPass(m_context, m_shaderDatabase, m_descriptorSetAllocator));
	m_pathTracingPass = std::unique_ptr<PathTracingPass>(new PathTracingPass(m_raytracingContext, m_shaderDatabase, m_descriptorSetAllocator));
	m_gbufferLayoutPass = std::unique_ptr<GBufferLayoutPass>(new GBufferLayoutPass(m_context, m_shaderDatabase, m_descriptorSetAllocator));
	m_gbufferSamplePass = std::unique_ptr<GBufferSamplePass>(new GBufferSamplePass(m_context, m_shaderDatabase, m_descriptorSetAllocator));
	m_directIlluminationPass = std::unique_ptr<DirectIlluminationPass>(new DirectIlluminationPass(m_raytracingContext, m_shaderDatabase, m_descriptorSetAllocator));
	m_deferredShadingPass = std::unique_ptr<DeferredShadingPass>(new DeferredShadingPass(m_context, m_shaderDatabase, m_descriptorSetAllocator));
	m_presentPass = std::unique_ptr<PresentPass>(new PresentPass(m_context, m_shaderDatabase, m_descriptorSetAllocator));
	m_uiPass = std::unique_ptr<UIPass>(new UIPass(m_context, m_descriptorSetAllocator.fixedPool()));
}

void Renderer::recreateSwapDependentResources(const vkb::Swapchain& swapchain)
{
	m_rngGenPass->recreateResources(swapchain.extent);
	m_pathTracingPass->recreateResources(swapchain.extent);
	m_gbufferLayoutPass->loDefLODPassResources->recreateResources();
	m_gbufferLayoutPass->hiDefLODPassResources->recreateResources();
	m_gbufferSamplePass->passResources->recreateResources();
	m_directIlluminationPass->recreateResources(swapchain.extent);
	m_deferredShadingPass->passResources->recreateResources();
	m_presentPass->passResources->recreateResources();
	m_uiPass->passResources->recreateResources();

	// XXX: hacky way to ensure resources are valid, check if this should be done differently
	prepareFrame();
}
