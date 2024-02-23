#pragma once

#include <hybrid_renderer.h>
#include <memory>

#include "subsystems.h"

/// @brief The Renderer Frame Data structure contains per frame data for rendering.
struct RendererFrameData
{
	std::unique_ptr<hri::BufferResource> cameraUBO;
	std::unique_ptr<hri::DescriptorSetManager> sceneDataSet;
	std::unique_ptr<hri::DescriptorSetManager> presentInputSet;
};

class Renderer
{
public:
	Renderer(hri::RenderContext& ctx, hri::Camera& camera);

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

	void prepareFrameResources(const hri::ActiveFrame& frame);

private:
	hri::RenderContext& m_context;
	hri::RenderCore m_renderCore;
	hri::ShaderDatabase m_shaderDatabase;
	hri::RenderSubsystemManager m_subsystemManager;
	hri::DescriptorSetAllocator m_descriptorSetAllocator;

	// Renderer state
	hri::Camera& m_camera;

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
	std::unique_ptr<UISubsystem> m_uiSubsystem;
	std::unique_ptr<PresentationSubsystem> m_presentSubsystem;

	// Renderer per frame data
	RendererFrameData m_frames[hri::RenderCore::framesInFlight()] = {};
};
