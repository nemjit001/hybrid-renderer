#pragma once

#include <cassert>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "renderer_internal/render_context.h"
#include "renderer_internal/shader_database.h"

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

	class RasterFrameGraphNode
		:
		public IFrameGraphNode
	{
	public:
		virtual void execute(VkCommandBuffer commandBuffer) const override
		{
			assert(m_pPSO != nullptr);

			// TODO: bind resources
			vkCmdBindPipeline(commandBuffer, m_pPSO->bindPoint, m_pPSO->pipeline);
			// TODO: bind buffers
			// TODO: draw
		}
	};

	class PresentFrameGraphNode
		:
		public IFrameGraphNode
	{
	public:
		virtual void execute(VkCommandBuffer commandBuffer) const override
		{
			assert(m_pPSO != nullptr);

			// TODO: bind resources
			vkCmdBindPipeline(commandBuffer, m_pPSO->bindPoint, m_pPSO->pipeline);
			vkCmdDraw(commandBuffer, 3, 1, 0, 0);
		}
	};

	class FrameGraph
	{
	public:
		FrameGraph(RenderContext* ctx, ShaderDatabase* shaderDB);

		virtual ~FrameGraph();

		void execute(VkCommandBuffer commandBuffer, uint32_t activeSwapImageIdx) const;

		void recreateFrameResources();

	private:
		void createRenderPass();

		void createFrameResources();

		void destroyFrameResources();

		GraphicsPipelineBuilder getPresentPipelineBuilder() const;

	private:
		RenderContext* m_pCtx						= nullptr;

		// FIXME: auto generate render targets based on Frame Graph state.
		RenderTarget m_gbufferDepthTarget			= RenderTarget{};
		RenderTarget m_gbufferNormalTarget			= RenderTarget{};
		RenderTarget m_gbufferAlbedoTarget			= RenderTarget{};

		// FIXME: auto generate render pass based on frame graph nodes
		VkRenderPass m_renderPass					= VK_NULL_HANDLE;

		// TODO: only create framebuffers after render pass & target auto generation
		std::vector<VkImageView> m_swapViews		= {};
		std::vector<VkFramebuffer> m_framebuffers	= {};

		// FIXME: remove default builtins, instead allow graph setup by API users
		PresentFrameGraphNode m_presentNode			= PresentFrameGraphNode();
	};
}
