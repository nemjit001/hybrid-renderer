#pragma once

#include <hybrid_renderer.h>
#include <memory>

#include "detail/raytracing.h"
#include "scene.h"

/// @brief Common render resources used by subpasses
struct CommonResources
{
	uint32_t frameIndex;
	uint32_t subFrameIndex;
	SceneGraph* activeScene;
	std::unique_ptr<hri::BufferResource> prevCameraUBO;
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

/// @brief Simple RNG source. White Noise
class RngGenerationPass
	:
	public IRenderPass
{
public:
	struct PushConstantData
	{
		uint32_t frameIndex;
	};

public:
	RngGenerationPass(hri::RenderContext& ctx, hri::ShaderDatabase& shaderDB, hri::DescriptorSetAllocator& descriptorAllocator);

	virtual ~RngGenerationPass();

	virtual void prepareFrame(CommonResources& resources) override;

	virtual void drawFrame(hri::ActiveFrame& frame, CommonResources& resources) override;

	void recreateResources(VkExtent2D resolution);

public:
	std::unique_ptr<hri::DescriptorSetLayout> rngDescriptorSetLayout;
	std::unique_ptr<hri::DescriptorSetManager> rngDescriptorSet;
	std::unique_ptr<hri::ImageResource> rngSource;

private:
	VkPipelineLayout m_layout			= VK_NULL_HANDLE;
	hri::PipelineStateObject* m_pPSO	= nullptr;
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
		HRI_ALIGNAS(4) uint32_t subFrameIdx;
	};

public:
	PathTracingPass(raytracing::RayTracingContext& ctx, hri::ShaderDatabase& shaderDB, hri::DescriptorSetAllocator& descriptorAllocator);

	virtual ~PathTracingPass();

	virtual void prepareFrame(CommonResources& resources) override;

	virtual void drawFrame(hri::ActiveFrame& frame, CommonResources& resources) override;

	void recreateResources(VkExtent2D resolution);

	inline VkImageView getRenderResultView() const { return renderResult[pingPong]->view; };

public:
	raytracing::RayTracingContext& rtContext;
	std::unique_ptr<hri::DescriptorSetLayout> sceneDescriptorSetLayout;
	std::unique_ptr<hri::DescriptorSetLayout> rtDescriptorSetLayout;
	std::unique_ptr<hri::DescriptorSetManager> sceneDescriptorSet;
	std::unique_ptr<hri::DescriptorSetManager> rtDescriptorSet;

	bool pingPong;	//< used to swap between render buffers
	std::unique_ptr<hri::ImageResource> renderResult[2];	//< Used to ping/pong previous frame data
	std::unique_ptr<hri::ImageResource> reprojectHistory;	//< Used for reprojection
	std::unique_ptr<hri::ImageResource> accumulator;		//< Used for simple accumulation of resources

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
public:
	enum class LODMode
	{
		LODNear,
		LODFar,
	};

	struct PushConstantData
	{
		HRI_ALIGNAS(4)  uint32_t instanceId;
		HRI_ALIGNAS(4)	uint32_t lodMask;
		HRI_ALIGNAS(16) hri::Float4x4 modelMatrix;
	};

public:
	GBufferLayoutPass(hri::RenderContext& ctx, hri::ShaderDatabase& shaderDB, hri::DescriptorSetAllocator& descriptorAllocator);

	virtual ~GBufferLayoutPass();

	virtual void prepareFrame(CommonResources& resources) override;

	virtual void drawFrame(hri::ActiveFrame& frame, CommonResources& resources) override;

private:
	void executeGBufferPass(hri::RenderPassResourceManager& resourceManager, hri::ActiveFrame& frame, CommonResources& resources, LODMode mode);

public:
	std::unique_ptr<hri::DescriptorSetLayout> sceneDescriptorSetLayout;
	std::unique_ptr<hri::DescriptorSetManager> sceneDescriptorSet;

	// 2 passes in one, for near & far LODs
	std::unique_ptr<hri::RenderPassResourceManager> loDefLODPassResources;
	std::unique_ptr<hri::RenderPassResourceManager> hiDefLODPassResources;

protected:
	VkPipelineLayout m_layout = VK_NULL_HANDLE;
	hri::PipelineStateObject* m_pPSO = nullptr;
};

/// @brief GBuffer sample pass that samples 2 GBuffer layouts and blends between them using stochastic sampling
class GBufferSamplePass
	:
	public IRenderPass
{
public:
	struct PushConstantData
	{
		HRI_ALIGNAS(16) hri::Float2 resolution;
	};

public:
	GBufferSamplePass(hri::RenderContext& context, hri::ShaderDatabase& shaderDB, hri::DescriptorSetAllocator& descriptorAllocator);

	virtual ~GBufferSamplePass();

	virtual void drawFrame(hri::ActiveFrame& frame, CommonResources& resources) override;

public:
	// Pass sampler
	std::unique_ptr<hri::ImageSampler> passInputSampler;

	// Descriptor set layouts
	std::unique_ptr<hri::DescriptorSetLayout> rngDescriptorSetLayout;
	std::unique_ptr<hri::DescriptorSetLayout> gbufferSampleDescriptorSetLayout;

	// Descriptor sets
	std::unique_ptr<hri::DescriptorSetManager> rngDescriptorSet;
	std::unique_ptr<hri::DescriptorSetManager> loDefDescriptorSet;
	std::unique_ptr<hri::DescriptorSetManager> hiDefDescriptorSet;

	// Pass resources
	std::unique_ptr<hri::RenderPassResourceManager> passResources;

protected:
	VkPipelineLayout m_layout			= VK_NULL_HANDLE;
	hri::PipelineStateObject* m_pPSO	= nullptr;
};

/// @brief Direct illumination ray tracing pass
class DirectIlluminationPass
	:
	public IRenderPass
{
public:
	DirectIlluminationPass(raytracing::RayTracingContext& ctx, hri::ShaderDatabase& shaderDB, hri::DescriptorSetAllocator& descriptorAllocator);

	virtual ~DirectIlluminationPass();

	virtual void prepareFrame(CommonResources& resources) override;

	virtual void drawFrame(hri::ActiveFrame& frame, CommonResources& resources) override;

	void recreateResources(VkExtent2D resolution);

public:
	raytracing::RayTracingContext& rtContext;

	// Pass sampler
	std::unique_ptr<hri::ImageSampler> passInputSampler;
	
	// Descriptor set layouts
	std::unique_ptr<hri::DescriptorSetLayout> gbufferDataDescriptorSetLayout;
	std::unique_ptr<hri::DescriptorSetLayout> rtDescriptorSetLayout;

	// Descriptor sets
	std::unique_ptr<hri::DescriptorSetManager> gbufferDataDescriptorSet;
	std::unique_ptr<hri::DescriptorSetManager> rtDescriptorSet;

	// Image handles
	std::unique_ptr<hri::ImageResource> renderResult;

protected:
	VkPipelineLayout m_layout = VK_NULL_HANDLE;
	hri::PipelineStateObject* m_pPSO = nullptr;
	std::unique_ptr<raytracing::ShaderBindingTable> m_SBT;
};

/// @brief Deferred shading pass, combines sampled GBuffer & Direct Illumination pass for a final render result
class DeferredShadingPass
	:
	public IRenderPass
{
public:
	DeferredShadingPass(hri::RenderContext& ctx, hri::ShaderDatabase& shaderDB, hri::DescriptorSetAllocator& descriptorAllocator);

	virtual ~DeferredShadingPass();

	virtual void drawFrame(hri::ActiveFrame& frame, CommonResources& resources) override;

public:
	// Pass sampler
	std::unique_ptr<hri::ImageSampler> passInputSampler;

	// Descriptor set stuff
	std::unique_ptr<hri::DescriptorSetLayout> inputDescriptorSetLayout;
	std::unique_ptr<hri::DescriptorSetManager> inputDescriptorSet;

	// Pass resources
	std::unique_ptr<hri::RenderPassResourceManager> passResources;

protected:
	VkPipelineLayout m_layout = VK_NULL_HANDLE;
	hri::PipelineStateObject* m_pPSO = nullptr;
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
