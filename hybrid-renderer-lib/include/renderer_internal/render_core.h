#pragma once

#include <functional>

#include "config.h"
#include "platform.h"
#include "renderer_internal/render_context.h"

namespace hri
{
	typedef std::function<void(VkCommandBuffer)> HRIImmediateSubmitFunc;
	typedef std::function<void(const vkb::Swapchain&)> HRIOnSwapchainInvalidateFunc;

	/// @brief The Frame State manages per frame data such as buffers & sync primitives
	struct FrameState
	{
		VkFence frameReady						= VK_NULL_HANDLE;
		VkSemaphore imageAvailable				= VK_NULL_HANDLE;
		VkSemaphore renderingFinished			= VK_NULL_HANDLE;
		VkCommandPool graphicsCommandPool		= VK_NULL_HANDLE;
		VkCommandBuffer graphicsCommandBuffer	= VK_NULL_HANDLE;

		static FrameState init(RenderContext* ctx);

		static void destroy(RenderContext* ctx, FrameState& frameState);
	};

	/// @brief The Active Frame struct maintains information on frame state relevant for recording graphics commands.
	struct ActiveFrame
	{
		uint32_t activeSwapImageIndex	= 0;
		VkCommandBuffer commandBuffer	= VK_NULL_HANDLE;

		/// @brief Begin rendering commands for this frame.
		inline void beginCommands()
		{
			VkCommandBufferBeginInfo beginInfo = VkCommandBufferBeginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
			beginInfo.flags = 0;
			beginInfo.pInheritanceInfo = nullptr;
			vkBeginCommandBuffer(commandBuffer, &beginInfo);
		}

		/// @brief End rendering commands for this frame.
		inline void endCommands()
		{
			HRI_VK_CHECK(vkEndCommandBuffer(commandBuffer));
		}

		/// @brief Insert an image memory barrier in the frame.
		/// @param image Image for which this barrier is active.
		/// @param srcStage Source pipeline stage.
		/// @param dstStage Destination pipeline stage.
		/// @param srcAccessMask Source access flags, when the image can be transitioned.
		/// @param dstAccessMask Destination access flags, when the image has to be done transitioning.
		/// @param oldLayout Old image layout.
		/// @param newLayout New image layout.
		/// @param subresourceRange Image subresource range to transition.
		/// @param srcQueueFamily Current queue family of image.
		/// @param dstQueueFamily Target queue family of image.
		void imageMemoryBarrier(
			VkImage image,
			VkPipelineStageFlags srcStage,
			VkPipelineStageFlags dstStage,
			VkAccessFlags srcAccessMask,
			VkAccessFlags dstAccessMask,
			VkImageLayout oldLayout,
			VkImageLayout newLayout,
			VkImageSubresourceRange subresourceRange,
			uint32_t srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
			uint32_t dstQueueFamily = VK_QUEUE_FAMILY_IGNORED
		) const;

		/// @brief Insert a buffer memory barrier in the frame.
		/// @param buffer Buffer for which this barrier is active.
		/// @param srcStage Source pipeline stage.
		/// @param dstStage Destination pipeline stage.
		/// @param srcAccessMask Source access flags, when the buffer is no longer accessed.
		/// @param dstAccessMask Destination access flags, when the buffer has to be accessible again.
		/// @param offset Buffer region offset for the barrier.
		/// @param size Buffer region size for the barrier, may be VK_WHOLE_SIZE.
		/// @param srcQueueFamily Current queue family of image.
		/// @param dstQueueFamily Target queue family of image.
		void bufferMemoryBarrier(
			VkBuffer buffer,
			VkPipelineStageFlags srcStage,
			VkPipelineStageFlags dstStage,
			VkAccessFlags srcAccessMask,
			VkAccessFlags dstAccessMask,
			size_t offset,
			size_t size,
			uint32_t srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
			uint32_t dstQueueFamily = VK_QUEUE_FAMILY_IGNORED
		) const;

		/// @brief Insert a memory barrier in the frame.
		/// @param srcStage Source pipeline stage.
		/// @param dstStage Destination pipeline stage.
		/// @param srcAccessMask Source access flags, when the memory is no longer accessed.
		/// @param dstAccessMask Destination access flags, when the memory has to be accessible again.
		void memoryBarrier(
			VkPipelineStageFlags srcStage,
			VkPipelineStageFlags dstStage,
			VkAccessFlags srcAccessMask,
			VkAccessFlags dstAccessMask
		) const;
	};

	/// @brief The Render Core handles frame state and work submission.
	class RenderCore
	{
	public:
		/// @brief Create a new RenderCore instance.
		/// @param ctx The render context to use for this Renderer.
		RenderCore(RenderContext* ctx);

		/// @brief Destroy this render core instance.
		virtual ~RenderCore();

		RenderCore(const RenderCore&) = delete;
		RenderCore& operator=(const RenderCore&) = delete;

		/// @brief Start a new frame.
		void startFrame();

		/// @brief End the currently active frame, submitting recorded work to the GPU.
		void endFrame();

		/// @brief Await the finish of the currently active frame.
		void awaitFrameFinished() const;

		/// @brief Register a callback for when the Swap Chain is invalidated. This callback can be used to
		///		recreate frame resources for example.
		/// @param onSwapchainInvalidate Callback to execute on swapchain invalidation.
		/// @return The previously registered callback, may be a nullptr.
		HRIOnSwapchainInvalidateFunc setOnSwapchainInvalidateCallback(HRIOnSwapchainInvalidateFunc onSwapchainInvalidate);

		/// @brief Immeditely record & submit Vulkan commands, e.g. for resource transfer operations.
		/// @param submitFunc The submit function to use.
		void immediateSubmit(HRIImmediateSubmitFunc submitFunc);

		/// @brief Retrive the currently active frame's data.
		/// @return ActiveFrame struct.
		inline const ActiveFrame getActiveFrame() const { return ActiveFrame{ m_activeSwapImage, m_frames[m_currentFrame].graphicsCommandBuffer }; }

	private:
		/// @brief Validate a swap chain operation result, setting the recreate flag if necessary.
		/// @param result A result value generated by a swap chain operation.
		void validateSwapchainState(VkResult result);

	private:
		RenderContext* m_pCtx			= nullptr;
		uint32_t m_previousFrame		= 0;
		uint32_t m_currentFrame			= 0;
		uint32_t m_activeSwapImage		= 0;
		bool m_recreateSwapchain		= false;
		VkCommandPool m_submitPool		= VK_NULL_HANDLE;
		HRIOnSwapchainInvalidateFunc m_onSwapchainInvalidateFunc = nullptr;
		FrameState m_frames[HRI_VK_FRAMES_IN_FLIGHT] = {};
	};
}
