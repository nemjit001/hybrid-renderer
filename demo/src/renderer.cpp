#include "renderer.h"

#include <hybrid_renderer.h>
#include <memory>

#include "detail/raytracing.h"
#include "render_passes.h"

Renderer::Renderer(raytracing::RayTracingContext& ctx, hri::Camera& camera, SceneGraph& activeScene)
	:
	m_context(ctx.renderContext),
	m_raytracingContext(ctx),
	m_renderCore(m_context),
	m_shaderDatabase(m_context),
	m_descriptorSetAllocator(m_context),
	m_computePool(m_context, m_context.queues.computeQueue, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT),
	m_stagingPool(m_context, m_context.queues.transferQueue, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT),
	m_accelerationStructureManager(ctx),
	m_frameCounter(0),
	m_camera(camera),
	m_activeScene(activeScene)
{
	assert(m_activeScene.lightCount > 0);

	// Set up frame resources
	auto instances = m_activeScene.generateRenderInstanceList(m_camera);
	m_frameResources = CommonResources{};
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

void Renderer::prepareFrame()
{
	m_renderCore.awaitFrameFinished();

	// update instance list & frame resource state
	auto instances = m_activeScene.generateRenderInstanceList(m_camera);
	m_frameResources.frameIndex = m_frameCounter;

	// Copy SSBO & UBO data to buffers and check if TLAS realloc is needed
	m_frameResources.cameraUBO->copyToBuffer(&m_camera.getShaderData(), sizeof(hri::CameraShaderData));
	if (m_accelerationStructureManager.shouldReallocTLAS(*m_frameResources.tlas, instances, m_frameResources.blasList))
		m_frameResources.tlas = std::make_unique<raytracing::AccelerationStructure>(m_accelerationStructureManager.createTLAS(instances, m_frameResources.blasList));

	// Build acceleration structures for this frame
	VkCommandBuffer ASBuildCommands = m_computePool.createCommandBuffer(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	m_accelerationStructureManager.cmdBuildBLASses(ASBuildCommands, instances, m_activeScene.meshes, m_frameResources.blasList);
	m_accelerationStructureManager.cmdBuildTLAS(ASBuildCommands, instances, m_frameResources.blasList, *m_frameResources.tlas);
	m_computePool.submitAndWait(ASBuildCommands);
	m_computePool.freeCommandBuffer(ASBuildCommands);

	// Prepare pass I/O descriptors
	VkDescriptorImageInfo renderResultInfo = VkDescriptorImageInfo{};
	renderResultInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	renderResultInfo.imageView = m_pathTracingPass->renderResult->view;
	renderResultInfo.sampler = m_presentPass->passInputSampler->sampler;
	(*m_presentPass->presentDescriptorSet)
		.writeImage(0, &renderResultInfo)
		.flush();

	// Prepare per pass frame resources
	m_pathTracingPass->prepareFrame(m_frameResources);
	m_presentPass->prepareFrame(m_frameResources);
	m_uiPass->prepareFrame(m_frameResources);
}

void Renderer::drawFrame()
{
	// TODO: debug frame timing output

	m_renderCore.startFrame();
	hri::ActiveFrame frame = m_renderCore.getActiveFrame();

	// Begin command recording for this frame
	frame.beginCommands();

	m_pathTracingPass->drawFrame(frame, m_frameResources);
	m_presentPass->drawFrame(frame, m_frameResources);
	m_uiPass->drawFrame(frame, m_frameResources);

	frame.endCommands();
	m_renderCore.endFrame();

	m_frameCounter += 1;
}

void Renderer::initRenderPasses()
{
	m_pathTracingPass = std::unique_ptr<PathTracingPass>(new PathTracingPass(m_raytracingContext, m_shaderDatabase, m_descriptorSetAllocator));
	m_presentPass = std::unique_ptr<PresentPass>(new PresentPass(m_context, m_shaderDatabase, m_descriptorSetAllocator));
	m_uiPass = std::unique_ptr<UIPass>(new UIPass(m_context, m_descriptorSetAllocator.fixedPool()));
}

void Renderer::recreateSwapDependentResources(const vkb::Swapchain& swapchain)
{
	m_pathTracingPass->recreateResources(swapchain.extent);
	m_presentPass->passResources->recreateResources();
	m_uiPass->passResources->recreateResources();

	// XXX: hacky way to ensure resources are valid, is there a better way?
	prepareFrame();
}
