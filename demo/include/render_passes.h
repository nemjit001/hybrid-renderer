#pragma once

#include <hybrid_renderer.h>
#include <memory>

#include "detail/raytracing.h"
#include "scene.h"

/// @brief Common render resources used by subpasses
struct CommonResources
{
	uint32_t frameIndex;
	std::unique_ptr<hri::BufferResource> cameraUBO;
	hri::BufferResource* instanceDataSSBO;
	hri::BufferResource* materialSSBO;
	std::vector<raytracing::AccelerationStructure> blasList;
	std::unique_ptr<raytracing::AccelerationStructure> tlas;
};

/// @brief Base render pass interface
class IRenderPass
{
public:
	IRenderPass(hri::RenderContext& ctx);

	virtual ~IRenderPass() = default;

	virtual void prepareFrame(CommonResources& resources);

	virtual void drawFrame(hri::ActiveFrame& frame, CommonResources& resources) = 0;

public:
	hri::RenderContext& context;
	hri_debug::DebugHandler debug = hri_debug::DebugHandler(context);
};

/// @brief Path tracing render pass
class PathTracingPass
	:
	public IRenderPass
{
public:
	struct PushConstantData
	{
		HRI_ALIGNAS(4) uint32_t frameIdx;
	};

public:
	PathTracingPass(raytracing::RayTracingContext& ctx, hri::ShaderDatabase& shaderDB, hri::DescriptorSetAllocator& descriptorAllocator);

	virtual ~PathTracingPass();

	virtual void prepareFrame(CommonResources& resources) override;

	virtual void drawFrame(hri::ActiveFrame& frame, CommonResources& resources) override;

	void recreateResources(VkExtent2D resolution);

public:
	raytracing::RayTracingContext& rtContext;
	std::unique_ptr<hri::DescriptorSetLayout> sceneDescriptorSetLayout;
	std::unique_ptr<hri::DescriptorSetLayout> rtDescriptorSetLayout;
	std::unique_ptr<hri::DescriptorSetManager> sceneDescriptorSet;
	std::unique_ptr<hri::DescriptorSetManager> rtDescriptorSet;
	std::unique_ptr<hri::ImageResource> renderResult;

protected:
	VkPipelineLayout m_layout			= VK_NULL_HANDLE;
	hri::PipelineStateObject* m_pPSO	= nullptr;
	std::unique_ptr<raytracing::ShaderBindingTable> m_SBT;
};

/// @brief GBuffer layout render pass
class GBufferLayoutPass
	:
	public IRenderPass
{
	//
};

/// @brief GBuffer sample pass that samples 2 GBuffer layouts and blends between them using stochastic sampling
class GBufferSamplePass
	:
	public IRenderPass
{
	//
};

/// @brief Direct illumination ray tracing pass
class DirectIlluminationPass
	:
	public IRenderPass
{
	//
};

/// @brief Deferred shading pass, combines sampled GBuffer & Direct Illumination pass for a final render result
class DeferredShadingPass
	:
	public IRenderPass
{
	//
};

/// @brief Present pass, draws an image to a window surface
struct PresentPass
	:
	public IRenderPass
{
public:
	PresentPass(hri::RenderContext& ctx, hri::ShaderDatabase& shaderDB, hri::DescriptorSetAllocator& descriptorAllocator);

	virtual ~PresentPass();

	virtual void drawFrame(hri::ActiveFrame& frame, CommonResources& resources) override;

public:
	std::unique_ptr<hri::ImageSampler> passInputSampler;
	std::unique_ptr<hri::DescriptorSetLayout> presentDescriptorSetLayout;
	std::unique_ptr<hri::DescriptorSetManager> presentDescriptorSet;
	std::unique_ptr<hri::SwapchainPassResourceManager> passResources;

protected:
	VkPipelineLayout m_layout			= VK_NULL_HANDLE;
	hri::PipelineStateObject* m_pPSO	= nullptr;
};

/// @brief UI pass, draws UI over window surface
class UIPass
	:
	public IRenderPass
{
public:
	UIPass(hri::RenderContext& ctx, VkDescriptorPool uiPool);

	virtual ~UIPass();

	virtual void drawFrame(hri::ActiveFrame& frame, CommonResources& resources) override;

public:
	std::unique_ptr<hri::SwapchainPassResourceManager> passResources;
};
