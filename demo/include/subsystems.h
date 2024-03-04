#pragma once

#include <hybrid_renderer.h>
#include <memory>

#include "detail/raytracing.h"
#include "scene.h"

struct GBufferLayoutFrameInfo
{
	VkDescriptorSet sceneDataSetHandle	= VK_NULL_HANDLE;
	SceneGraph* pSceneGraph				= nullptr;
};

struct RayTracingFrameInfo
{
	uint32_t frameCounter						= 0;

	// gbuffer targets
	hri::ImageResource* gbufferAlbedo			= nullptr;
	hri::ImageResource* gbufferEmission			= nullptr;
	hri::ImageResource* gbufferSpecular			= nullptr;
	hri::ImageResource* gbufferTransmittance	= nullptr;
	hri::ImageResource* gbufferNormal			= nullptr;
	hri::ImageResource* gbufferDepth			= nullptr;

	// current frame targets
	hri::ImageResource* globalIlluminationOut	= nullptr;

	// Descriptor sets
	VkDescriptorSet sceneDataSetHandle			= VK_NULL_HANDLE;
	VkDescriptorSet raytracingSetHandle			= VK_NULL_HANDLE;
};

struct DeferredShadingFrameInfo
{
	VkDescriptorSet deferedShadingSetHandle		= VK_NULL_HANDLE;
};

struct UIFrameInfo
{
	//
};

struct PresentFrameInfo
{
	VkDescriptorSet presentInputSetHandle	= VK_NULL_HANDLE;
};

/// @brief The GBuffer Layout Subsystem handles drawing a scene and storing the resulting geometry data (depth, world position, normal, etc.)
///		in its pass render attachments.
class GBufferLayoutSubsystem
	:
	public hri::IRenderSubsystem<GBufferLayoutFrameInfo>
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
	virtual void record(hri::ActiveFrame& frame, GBufferLayoutFrameInfo& frameInfo) const override;
};

class IRayTracingSubSystem
	:
	public hri::IRenderSubsystem<RayTracingFrameInfo>
{
public:
	IRayTracingSubSystem(raytracing::RayTracingContext& ctx);

	virtual ~IRayTracingSubSystem() = default;

	void transitionGbufferResources(hri::ActiveFrame& frame, RayTracingFrameInfo& frameInfo) const;

protected:
	raytracing::RayTracingContext& m_rtCtx;
	std::unique_ptr<raytracing::ShaderBindingTable> m_SBT;
};

class GlobalIlluminationSubsystem
	:
	public IRayTracingSubSystem
{
public:
	GlobalIlluminationSubsystem(
		raytracing::RayTracingContext& ctx,
		hri::ShaderDatabase& shaderDB,
		VkDescriptorSetLayout sceneDataSetLayout,
		VkDescriptorSetLayout rtSetLayout
	);

	virtual ~GlobalIlluminationSubsystem() = default;

	virtual void record(hri::ActiveFrame& frame, RayTracingFrameInfo& frameInfo) const override;
};

/// @brief The UI Subsystem handles drawing an UI overlay to the swap render pass.
class UISubsystem
	:
	public hri::IRenderSubsystem<UIFrameInfo>
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
	virtual void record(hri::ActiveFrame& frame, UIFrameInfo& frameInfo) const override;
};

class DeferredShadingSubsystem
	:
	public hri::IRenderSubsystem<DeferredShadingFrameInfo>
{
public:
	DeferredShadingSubsystem(
		hri::RenderContext& ctx,
		hri::ShaderDatabase& shaderDB,
		VkRenderPass renderPass,
		VkDescriptorSetLayout composeSetLayout
	);

	virtual ~DeferredShadingSubsystem() = default;

	virtual void record(hri::ActiveFrame& frame, DeferredShadingFrameInfo& frameInfo) const override;
};

/// @brief The Presentation Subsystem handles drawing offscreen render results to a swap image.
class PresentationSubsystem
	:
	public hri::IRenderSubsystem<PresentFrameInfo>
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
	virtual void record(hri::ActiveFrame& frame, PresentFrameInfo& frameInfo) const override;
};
