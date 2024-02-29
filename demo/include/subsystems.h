#pragma once

#include <hybrid_renderer.h>
#include <memory>

#include "detail/raytracing.h"
#include "scene.h"

/// @brief The Instance Push Constant is used to push instance & transformation data for renderable objects to shaders.
struct InstancePushConstanct
{
	HRI_ALIGNAS(4)  uint32_t instanceId;
	HRI_ALIGNAS(16) hri::Float4x4 model 	= hri::Float4x4(1.0f);
	HRI_ALIGNAS(16) hri::Float3x3 normal 	= hri::Float3x3(1.0f);
};

struct GBufferLayoutFrameInfo
{
	VkDescriptorSet sceneDataSetHandle	= VK_NULL_HANDLE;
	SceneGraph* pSceneGraph				= nullptr;
};

struct RayTracingFrameInfo
{
	VkDescriptorSet sceneDataSetHandle	= VK_NULL_HANDLE;
	VkDescriptorSet raytracingSetHandle = VK_NULL_HANDLE;
};

struct PresentFrameInfo
{
	VkDescriptorSet presentInputSetHandle = VK_NULL_HANDLE;
};

/// @brief The GBuffer Layout Subsystem handles drawing a scene and storing the resulting geometry data (depth, world position, normal, etc.)
///		in its pass render attachments.
class GBufferLayoutSubsystem
	:
	public hri::IRenderSubsystem
{
public:
	/// @brief Create a new GBuffer Layout subsystem.
	/// @param ctx Render Context to use.
	/// @param shaderDB Shader database to use.
	/// @param renderPass Render pass to use.
	/// @param sceneDataSetLayout Descriptor set layout describing scene data provided by renderer.
	GBufferLayoutSubsystem(
		hri::RenderContext& ctx,
		hri::ShaderDatabase& shaderDB,
		VkRenderPass renderPass,
		VkDescriptorSetLayout sceneDataSetLayout
	);

	/// @brief Destroy the GBufferLayoutSubsystem.
	virtual ~GBufferLayoutSubsystem() = default;

	/// @brief Record GBuffer Layout render commands.
	/// @param frame Active Frame to record into.
	virtual void record(hri::ActiveFrame& frame) const override;

	/// @brief Update frame info for this subsystem. The FrameInfo will be used for any subsequent command recordings.
	/// @param frameInfo FrameInfo to pass to the subsystem.
	inline void updateFrameInfo(const GBufferLayoutFrameInfo& frameInfo) { m_currentFrameInfo = frameInfo; }

protected:
	GBufferLayoutFrameInfo m_currentFrameInfo = GBufferLayoutFrameInfo{};
};

class IRayTracingSubSystem
	:
	public hri::IRenderSubsystem
{
public:
	IRayTracingSubSystem(raytracing::RayTracingContext& ctx);

	virtual ~IRayTracingSubSystem() = default;

	inline void updateFrameInfo(const RayTracingFrameInfo& frameInfo) { m_currentFrameInfo = frameInfo; }

protected:
	void initSBT(
		VkPipeline pipeline,
		uint32_t missGroupCount,
		uint32_t hitGroupCount,
		uint32_t callGroupCount = 0
	);

protected:
	raytracing::RayTracingContext& m_rtCtx;
	std::unique_ptr<raytracing::ShaderBindingTable> m_SBT;
	RayTracingFrameInfo m_currentFrameInfo = RayTracingFrameInfo{};
};

class HybridRayTracingSubsystem
	:
	public IRayTracingSubSystem
{
public:
	HybridRayTracingSubsystem(
		raytracing::RayTracingContext& ctx,
		hri::ShaderDatabase& shaderDB,
		VkDescriptorSetLayout sceneDataSetLayout,
		VkDescriptorSetLayout rtSetLayout
	);

	virtual ~HybridRayTracingSubsystem() = default;

	virtual void record(hri::ActiveFrame& frame) const override;
};

/// @brief The UI Subsystem handles drawing an UI overlay to the swap render pass.
class UISubsystem
	:
	public hri::IRenderSubsystem
{
public:
	/// @brief Create a new UI Subsystem.
	/// @param ctx Render Context to use.
	/// @param renderPass Render pass to use. Preferably drawing straight to swapchain.
	/// @param uiPool Descriptor pool to use for UI resources.
	UISubsystem(
		hri::RenderContext& ctx,
		VkRenderPass renderPass,
		VkDescriptorPool uiPool
	);

	/// @brief Destroy the UI Subsystem.
	virtual ~UISubsystem();

	/// @brief Record UI render commands.
	/// @param frame Active Frame to record into.
	virtual void record(hri::ActiveFrame& frame) const override;
};

/// @brief The Presentation Subsystem handles drawing offscreen render results to a swap image.
class PresentationSubsystem
	:
	public hri::IRenderSubsystem
{
public:
	/// @brief Create a new Presentation Subsystem.
	/// @param ctx Render Context to use.
	/// @param shaderDB Shader database to use.
	/// @param renderPass Render pass to use.
	/// @param presentInputSetLayout Descriptor set layout describing input images for presentation.
	PresentationSubsystem(
		hri::RenderContext& ctx,
		hri::ShaderDatabase& shaderDB,
		VkRenderPass renderPass,
		VkDescriptorSetLayout presentInputSetLayout
	);

	/// @brief Default destructor
	virtual ~PresentationSubsystem() = default;

	/// @brief Record Present render commands.
	/// @param frame Active Frame to record into.
	virtual void record(hri::ActiveFrame& frame) const override;

	/// @brief Update frame info for this subsystem. The FrameInfo will be used for any subsequent command recordings.
	/// @param frameInfo FrameInfo to pass to the subsystem.
	inline void updateFrameInfo(const PresentFrameInfo& frameInfo) { m_currentFrameInfo = frameInfo; }

protected:
	PresentFrameInfo m_currentFrameInfo = PresentFrameInfo{};
};
