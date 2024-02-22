#pragma once

#include <hybrid_renderer.h>
#include <memory>

#include "subsystems.h"

class Renderer
{
public:
	Renderer(hri::RenderContext& ctx, hri::Camera& camera, hri::Scene& scene);

	virtual ~Renderer();

	void setActiveScene(hri::Scene& scene);

	void drawFrame();

private:
	void initShaderDB();

	void initRenderPasses();

	void initSharedResources();

	void initGlobalDescriptorSets();

	void initRenderSubsystems();

	void recreateSwapDependentResources();

private:
	hri::RenderContext& m_context;
	hri::RenderCore m_renderCore;
	hri::ShaderDatabase m_shaderDatabase;
	hri::RenderSubsystemManager m_subsystemManager;
	hri::DescriptorSetAllocator m_descriptorSetAllocator;

	// Renderer state
	hri::Camera& m_worldCam;
	hri::Scene& m_activeScene;
	hri::BatchedSceneData m_batchedSceneData;

	// Shared samplers for render result sampling
	std::unique_ptr<hri::ImageSampler> m_renderResultLinearSampler;

	// Shared resources for passes
	hri::BufferResource m_worldCameraUBO;

	// Global descriptor sets & layouts
	std::unique_ptr<hri::DescriptorSetLayout> m_sceneDataSetLayout;
	std::unique_ptr<hri::DescriptorSetManager> m_sceneDataSet;
	std::unique_ptr<hri::DescriptorSetLayout> m_presentInputSetLayout;
	std::unique_ptr<hri::DescriptorSetManager> m_presentInputSet;

	// Render pass managers
	std::unique_ptr<hri::RenderPassResourceManager> m_gbufferLayoutPassManager;
	std::unique_ptr<hri::SwapchainPassResourceManager> m_presentPassManager;

	// Render subsystems
	std::unique_ptr<GBufferLayoutSubsystem> m_gbufferLayoutSubsystem;
	std::unique_ptr<UISubsystem> m_uiSubsystem;
	std::unique_ptr<PresentationSubsystem> m_presentSubsystem;
};
