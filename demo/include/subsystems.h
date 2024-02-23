#pragma once

#include <hybrid_renderer.h>
#include <vector>

/// @brief The Transform Push Constant is used to push transformation data for renderable objects to shaders.
struct TransformPushConstant
{
	HRI_ALIGNAS(16) hri::Float4x4 model 	= hri::Float4x4(1.0f);
	HRI_ALIGNAS(16) hri::Float3x3 normal 	= hri::Float3x3(1.0f);
};

struct GBufferLayoutFrameInfo
{
	VkDescriptorSet sceneDataSetHandle 	= VK_NULL_HANDLE;
	hri::RenderableScene* scene			= nullptr;
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
		hri::RenderContext* ctx,
		hri::ShaderDatabase* shaderDB,
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
		hri::RenderContext* ctx,
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
		hri::RenderContext* ctx,
		hri::ShaderDatabase* shaderDB,
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
