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
	for (uint32_t frame = 0; frame < hri::RenderCore::framesInFlight(); frame++)
	{
		CommonResources resources = CommonResources{};
		resources.camera = m_camera;
		resources.blasList = m_accelerationStructureManager.createBLASList(m_activeScene.meshes);
		resources.tlas = std::make_unique<raytracing::AccelerationStructure>(m_accelerationStructureManager.createTLAS(instances, resources.blasList));

		m_frameResources.push_back(std::move(resources));
	}

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
	uint32_t resourceIdx = m_frameCounter % hri::RenderCore::framesInFlight();
	CommonResources& frameResources = m_frameResources[resourceIdx];

	// update frame resource state
	frameResources.camera = m_camera;

	// generate new instance list & reallocate TLAS if needed
	auto instances = m_activeScene.generateRenderInstanceList(m_camera);
	if (m_accelerationStructureManager.shouldReallocTLAS(*frameResources.tlas, instances, frameResources.blasList))
		frameResources.tlas = std::make_unique<raytracing::AccelerationStructure>(m_accelerationStructureManager.createTLAS(instances, frameResources.blasList));

	// Build acceleration structures for this frame
	VkCommandBuffer ASBuildCommands = m_computePool.createCommandBuffer(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	m_accelerationStructureManager.cmdBuildBLASses(ASBuildCommands, instances, m_activeScene.meshes, frameResources.blasList);
	m_accelerationStructureManager.cmdBuildTLAS(ASBuildCommands, instances, frameResources.blasList, *frameResources.tlas);
	m_computePool.submitAndWait(ASBuildCommands);
	m_computePool.freeCommandBuffer(ASBuildCommands);

	// Update per pass resources
	m_pathTracingPass->prepareFrame(frameResources);
	m_presentPass->prepareFrame(frameResources);
	m_uiPass->prepareFrame(frameResources);
}

void Renderer::drawFrame()
{
	// TODO: debug frame timing output

	m_renderCore.startFrame();
	hri::ActiveFrame frame = m_renderCore.getActiveFrame();

	// Begin command recording for this frame
	frame.beginCommands();

	m_pathTracingPass->drawFrame(frame);
	m_presentPass->drawFrame(frame);
	m_uiPass->drawFrame(frame);

	frame.endCommands();
	m_renderCore.endFrame();

	m_frameCounter += 1;
}

void Renderer::initRenderPasses()
{
	m_pathTracingPass = std::unique_ptr<PathTracingPass>(new PathTracingPass(m_raytracingContext, m_shaderDatabase));
	m_presentPass = std::unique_ptr<PresentPass>(new PresentPass(m_context, m_shaderDatabase));
	m_uiPass = std::unique_ptr<UIPass>(new UIPass(m_context, m_descriptorSetAllocator.fixedPool()));
}

void Renderer::recreateSwapDependentResources(const vkb::Swapchain& swapchain)
{
	m_pathTracingPass->recreateResources(swapchain.extent);
	m_presentPass->passResources->recreateResources();
	m_uiPass->passResources->recreateResources();
}
