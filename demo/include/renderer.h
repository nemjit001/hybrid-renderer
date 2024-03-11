#pragma once

#include <hybrid_renderer.h>
#include <memory>

#include "detail/raytracing.h"
#include "scene.h"
#include "render_passes.h"

class Renderer
{
public:
	Renderer(raytracing::RayTracingContext& ctx, hri::Camera& camera, SceneGraph& activeScene);

	virtual ~Renderer();

	void setVSyncMode(hri::VSyncMode vsyncMode);

	void prepareFrame();

	void drawFrame();

private:
	void initRenderPasses();

	void recreateSwapDependentResources(const vkb::Swapchain& swapchain);

private:
	hri::RenderContext& m_context;
	raytracing::RayTracingContext& m_raytracingContext;
	hri::RenderCore m_renderCore;
	hri::ShaderDatabase m_shaderDatabase;
	hri::DescriptorSetAllocator m_descriptorSetAllocator;
	hri::CommandPool m_computePool;
	hri::CommandPool m_stagingPool;
	SceneASManager m_accelerationStructureManager;

	// Renderer state
	uint32_t m_frameCounter;
	hri::Camera& m_camera;
	SceneGraph& m_activeScene;
	CommonResources m_frameResources;

	// Render passes
	std::unique_ptr<PathTracingPass> m_pathTracingPass;
	std::unique_ptr<PresentPass> m_presentPass;
	std::unique_ptr<UIPass> m_uiPass;
};
