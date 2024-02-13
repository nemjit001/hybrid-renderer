#pragma once

#include <cassert>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "renderer_internal/render_context.h"

namespace hri
{
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
		virtual void execute(VkCommandBuffer) const = 0;

	private:
		// Pointer to pass resources (descriptor sets, push constant values)
		// Pointer to pipeline resources (graphics pipeline & its layout)
	};

	class RasterFrameGraphNode
		:
		public IFrameGraphNode
	{
	public:
		virtual void execute(VkCommandBuffer) const override
		{
			// TODO: record raster pass commands
			//	- bind pass resources
			//	- bind pass pipeline
			//	- bind pass buffers (vertex / index / inderect draw(?))
			//	- execute draw command(s)
		}
	};

	class PresentFrameGraphNode
		:
		public IFrameGraphNode
	{
	public:
		virtual void execute(VkCommandBuffer) const override
		{
			// TODO: record present pass commands
			//	- bind pass resources
			//	- bind pass pipeline
			//	- bind pass buffers (vertex / index / inderect draw(?))
			//	- execute draw command
		}
	};

	class FrameGraph
	{
	public:
		FrameGraph(RenderContext* ctx);

		virtual ~FrameGraph();

		void execute(VkCommandBuffer commandBuffer, uint32_t activeSwapImageIdx) const;

		void recreateFrameResources();

	private:
		void createRenderPass();

		void createFrameResources();

		void destroyFrameResources();

	private:
		RenderContext* m_pCtx						= nullptr;
		RenderTarget m_gbufferDepthTarget			= RenderTarget{};
		RenderTarget m_gbufferNormalTarget			= RenderTarget{};
		RenderTarget m_gbufferAlbedoTarget			= RenderTarget{};
		VkRenderPass m_renderPass					= VK_NULL_HANDLE;
		std::vector<VkImageView> m_swapViews		= {};
		std::vector<VkFramebuffer> m_framebuffers	= {};
	};
}
