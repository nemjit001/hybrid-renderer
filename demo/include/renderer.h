#pragma once

#include <hybrid_renderer.h>
#include <memory>

#include "detail/raytracing.h"
#include "scene.h"
#include "subsystems.h"

/// @brief The Renderer Frame Data structure contains per frame data for rendering.
struct RendererFrameData
{
	// Per frame resources
	std::unique_ptr<hri::BufferResource> cameraUBO;
	std::unique_ptr<raytracing::AccelerationStructure> tlas;
	std::vector<raytracing::AccelerationStructure> blasList;

	// Descriptor sets
	std::unique_ptr<hri::DescriptorSetManager> sceneDataSet;
	std::unique_ptr<hri::DescriptorSetManager> raytracingSet;
	std::unique_ptr<hri::DescriptorSetManager> presentInputSet;
};

enum SceneDataBindings
{
	Camera,
	RenderInstanceData,
	MaterialData,
};

enum RayTracingBindings
{
	Tlas,
	GBufferAlbedo,
	GBufferWorldPos,
	GBufferNormal,
	GBufferDepth,
	SoftShadowOutImage,
	DIOutImage,
};

enum ComposeInputBindings
{
	SoftShadowImage,
	DirectIlluminationImage,
};

enum PresentInputBindings
{
	RenderResult,
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

	void updateFrameDescriptors(RendererFrameData& frame);

	void prepareFrameResources(uint32_t frameIdx);

private:
	hri::RenderContext& m_context;
	raytracing::RayTracingContext& m_raytracingContext;
	hri_debug::DebugLabelHandler m_debug;
	hri::RenderCore m_renderCore;
	hri::ShaderDatabase m_shaderDatabase;
	hri::RenderSubsystemManager m_subsystemManager;
	hri::DescriptorSetAllocator m_descriptorSetAllocator;
	hri::CommandPool m_computePool;
	hri::CommandPool m_stagingPool;

	// Renderer state
	hri::Camera& m_camera;
	SceneGraph& m_activeScene;

	// Shared preinitialized samplers
	std::unique_ptr<hri::ImageSampler> m_renderResultNearestSampler;
	std::unique_ptr<hri::ImageSampler> m_renderResultLinearSampler;

	// Global descriptor set layouts
	std::unique_ptr<hri::DescriptorSetLayout> m_sceneDataSetLayout;
	std::unique_ptr<hri::DescriptorSetLayout> m_rtDescriptorSetLayout;
	std::unique_ptr<hri::DescriptorSetLayout> m_presentInputSetLayout;

	// Render pass managers
	std::unique_ptr<hri::RenderPassResourceManager> m_gbufferLayoutPassManager;
	std::unique_ptr<hri::SwapchainPassResourceManager> m_swapchainPassManager;

	// Ray Tracing Targets
	std::unique_ptr<hri::ImageResource> m_softShadowRTPassResult;
	std::unique_ptr<hri::ImageResource> m_directIlluminationRTPassResult;

	// Render subsystems
	std::unique_ptr<GBufferLayoutSubsystem> m_gbufferLayoutSubsystem;
	std::unique_ptr<HybridRayTracingSubsystem> m_hybridRTSubsystem;
	std::unique_ptr<UISubsystem> m_uiSubsystem;
	std::unique_ptr<PresentationSubsystem> m_presentSubsystem;

	// Renderer per frame data
	RendererFrameData m_frames[hri::RenderCore::framesInFlight()] = {};
};
