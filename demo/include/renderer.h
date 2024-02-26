#pragma once

#include <hybrid_renderer.h>
#include <memory>

#include "detail/raytracing.h"
#include "scene.h"
#include "subsystems.h"

/// @brief The Renderer Frame Data structure contains per frame data for rendering.
struct RendererFrameData
{
	std::unique_ptr<hri::BufferResource> cameraUBO;
	std::unique_ptr<hri::DescriptorSetManager> sceneDataSet;
	std::unique_ptr<hri::DescriptorSetManager> presentInputSet;
	std::vector<Renderable> renderables;
};

class Renderer
{
public:
	Renderer(raytracing::RayTracingContext& ctx, hri::Camera& camera, SceneGraph& activeScene);

	virtual ~Renderer();

	void setVSyncMode(hri::VSyncMode vsyncMode);

	void drawFrame();

private:
	void initShaderDB();

	void initRenderPasses();

	void initSharedResources();

	void initGlobalDescriptorSets();

	void initRenderSubsystems();

	void initRendererFrameData();

	void recreateSwapDependentResources();

	void prepareFrameResources(uint32_t frameIdx);

private:
	hri::RenderContext& m_context;
	raytracing::RayTracingContext& m_raytracingContext;
	hri::RenderCore m_renderCore;
	hri::ShaderDatabase m_shaderDatabase;
	hri::RenderSubsystemManager m_subsystemManager;
	hri::DescriptorSetAllocator m_descriptorSetAllocator;
	hri::CommandPool m_stagingPool;

	// Renderer state
	hri::Camera& m_camera;
	SceneGraph& m_activeScene;

	// Shared preinitialized samplers
	std::unique_ptr<hri::ImageSampler> m_renderResultLinearSampler;

	// Global descriptor set layouts
	std::unique_ptr<hri::DescriptorSetLayout> m_sceneDataSetLayout;
	std::unique_ptr<hri::DescriptorSetLayout> m_presentInputSetLayout;

	// Render pass managers
	std::unique_ptr<hri::RenderPassResourceManager> m_gbufferLayoutPassManager;
	std::unique_ptr<hri::SwapchainPassResourceManager> m_swapchainPassManager;

	// Render subsystems
	std::unique_ptr<GBufferLayoutSubsystem> m_gbufferLayoutSubsystem;
	std::unique_ptr<SoftShadowsRTSubsystem> m_softShadowsRTSubsystem;
	std::unique_ptr<UISubsystem> m_uiSubsystem;
	std::unique_ptr<PresentationSubsystem> m_presentSubsystem;

	// Renderer per frame data
	RendererFrameData m_frames[hri::RenderCore::framesInFlight()] = {};
};
