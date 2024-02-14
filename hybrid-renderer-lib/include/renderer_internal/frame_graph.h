#pragma once

#include <cassert>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "renderer_internal/render_context.h"
#include "renderer_internal/shader_database.h"

namespace hri
{
	/// @brief Simple viewport representation
	struct Viewport
	{
		float x			= 0.0f;
		float y			= 0.0f;
		float width		= 0.0f;
		float height	= 0.0f;
		float minDepth	= hri::DefaultViewportMinDepth;
		float maxDepth	= hri::DefaultViewportMaxDepth;
	};

	/// @brief Simple scissor rectangle representation
	struct Scissor
	{
		struct {
			int32_t x;
			int32_t y;
		};	// Offset

		struct {
			uint32_t width;
			uint32_t height;
		};	// Extent
	};

	/// @brief A Render Target is a collection of an image, its view, and an allocation.
	///		Render Targets are used by the Frame Graph to store intermediary render results.
	struct RenderTarget
	{
		VkImage image				= VK_NULL_HANDLE;
		VkImageView view			= VK_NULL_HANDLE;
		VmaAllocation allocation	= VK_NULL_HANDLE;

		/// @brief Initialize a new Render Target.
		/// @param ctx Render Context to use.
		/// @param format Target image format.
		/// @param extent Target extent (resolution)
		/// @param usage Target expected usage.
		/// @param imageAspect Target image aspect.
		/// @return A newly initialized RenderTarget.
		static RenderTarget init(
			RenderContext* ctx,
			VkFormat format,
			VkExtent2D extent,
			VkImageUsageFlags usage,
			VkImageAspectFlags imageAspect
		);

		/// @brief Destroy a Render Target.
		/// @param ctx Render Context to use.
		/// @param renderTarget Render Target to destroy.
		static void destroy(RenderContext* ctx, RenderTarget& renderTarget);
	};

	/// @brief The IFrameGraphNode interface exposes functions for rendering using a higher level interface.
	class IFrameGraphNode
	{
	public:
		/// @brief Overrideable execute method, this method is used to record command buffers for a node.
		/// @param commandBuffer The command buffer to record into.
		virtual void execute(VkCommandBuffer) const = 0;

		/// @brief Set the PSO for this graph node.
		/// @param pPSO PSO to register.
		virtual inline void setPipeline(const PipelineStateObject* pPSO) { assert(pPSO != nullptr); m_pPSO = pPSO; }

	protected:
		// Pointer to pass resources (descriptor sets, push constant values)
		const PipelineStateObject* m_pPSO	= nullptr;
	};

	/// @brief Raster type graph node, executes a rasterization pipeline.
	class RasterFrameGraphNode
		:
		public IFrameGraphNode
	{
	public:
		/// @brief Execute this raster pass.
		/// @param commandBuffer Command buffer to record into.
		virtual void execute(VkCommandBuffer commandBuffer) const override;

		/// @brief Set the viewport for this graph node.
		/// @param viewport The viewport to use.
		virtual void setViewport(Viewport viewport);

		/// @brief Set the scissor rectangle for this graph node.
		/// @param scissor The scissor rectangle to use.
		virtual void setScissor(Scissor scissor);

	protected:
		/// @brief Set the dynamic viewport state using constants stored in node.
		/// @param commandBuffer Command buffer to record into.
		virtual void setDynamicViewportState(VkCommandBuffer commandBuffer) const;

	protected:
		Viewport m_viewport		= Viewport{};
		Scissor m_scissor		= Scissor{};
	};

	/// @brief Present type raster graph node, executes a rasterization pipeline that writes into an active swap image.
	class PresentFrameGraphNode
		:
		public RasterFrameGraphNode
	{
	public:
		/// @brief Execute this present pass.
		/// @param commandBuffer Command buffer to record into.
		virtual void execute(VkCommandBuffer commandBuffer) const override;
	};

	/// @brief The Frame Graph generates a render pass & per frame render commands based on registered graph nodes.
	class FrameGraph
	{
	public:
		/// @brief Create a new Frame Graph.
		/// @param ctx Render context to use.
		FrameGraph(RenderContext* ctx);

		/// @brief Destroy this Frame Graph.
		virtual ~FrameGraph();

		/// @brief Execute the Frame Graph. NOTE: generate the frame graph before execution!
		/// @param commandBuffer Command buffer to record into.
		/// @param activeSwapImageIdx Active swap image to use as final output.
		void execute(VkCommandBuffer commandBuffer, uint32_t activeSwapImageIdx) const;

		/// @brief Generate a new Frame Graph from the registered nodes.
		void generateGraph();

	private:
		/// @brief Create frame graph resources such as frame buffers and render targets.
		void createFrameGraphResources();

		/// @brief Destroy the current frame graph resources.
		void destroyFrameGraphResources();

	private:
		RenderContext* m_pCtx						= nullptr;

		// TODO: auto generate render pass (w/ subpasses) & render targets

		std::vector<VkImageView> m_swapViews		= {};
		std::vector<VkFramebuffer> m_framebuffers	= {};
	};
}
