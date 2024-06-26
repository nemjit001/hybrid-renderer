#pragma once

#include <functional>

#include "config.h"
#include "platform.h"
#include "renderer_internal/render_context.h"

namespace hri
{
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
	///		NOTE: currentFrameIndex is in range [0, HRI_FRAMES_IN_FLIGHT - 1]
	struct ActiveFrame
	{
		uint32_t activeSwapImageIndex	= 0;
		uint32_t currentFrameIndex 		= 0;
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

		/// @brief Insert a memory pipeline barrier in the frame.
		/// @param memoryBarriers Memory Barriers list.
		/// @param flags Dependency flags to use.
		void pipelineBarrier(const std::vector<VkMemoryBarrier2>& memoryBarriers, VkDependencyFlags flags = 0) const;

		/// @brief Insert an image pipeline barrier in the frame.
		/// @param memoryBarriers Image Memory Barriers list.
		/// @param flags Dependency flags to use.
		void pipelineBarrier(const std::vector<VkImageMemoryBarrier2>& memoryBarriers, VkDependencyFlags flags = 0) const;
	};

	/// @brief The Render Core handles frame state and work submission.
	class RenderCore
	{
	public:
		/// @brief Create a new RenderCore instance.
		/// @param ctx The render context to use for this Renderer.
		RenderCore(RenderContext& ctx);

		/// @brief Destroy this render core instance.
		virtual ~RenderCore();

		// Disallow copy behaviour
		RenderCore(const RenderCore&) = delete;
		RenderCore& operator=(const RenderCore&) = delete;

		/// @brief Start a new frame.
		void startFrame();

		/// @brief End the currently active frame, submitting recorded work to the GPU.
		void endFrame();

		/// @brief Await the finish of the currently active frame.
		void awaitFrameFinished() const;

		/// @brief Await the finish of a frame.
		/// @param idx Set this to a frame idx to await the finish of a specific frame.
		void awaitFrameFinished(size_t idx) const;

		/// @brief Register a callback for when the Swap Chain is invalidated. This callback can be used to
		///		recreate frame resources for example.
		/// @param onSwapchainInvalidate Callback to execute on swapchain invalidation.
		/// @return The previously registered callback, may be a nullptr.
		HRIOnSwapchainInvalidateFunc setOnSwapchainInvalidateCallback(HRIOnSwapchainInvalidateFunc onSwapchainInvalidate);

		/// @brief Retrive the currently active frame's data.
		/// @return ActiveFrame struct.
		inline const ActiveFrame getActiveFrame() const { return ActiveFrame{ m_activeSwapImage, m_currentFrame, m_frames[m_currentFrame].graphicsCommandBuffer }; }

		/// @brief Retrieve number of frames in flight for this render core.
		/// @return The max number of frames in flight.
		inline static constexpr uint32_t framesInFlight() { return HRI_VK_FRAMES_IN_FLIGHT; }

	private:
		/// @brief Validate a swap chain operation result, setting the recreate flag if necessary.
		/// @param result A result value generated by a swap chain operation.
		void validateSwapchainState(VkResult result);

	private:
		RenderContext& m_ctx;
		uint32_t m_previousFrame		= 0;
		uint32_t m_currentFrame			= 0;
		uint32_t m_activeSwapImage		= 0;
		bool m_recreateSwapchain		= false;
		HRIOnSwapchainInvalidateFunc m_onSwapchainInvalidateFunc = nullptr;
		FrameState m_frames[HRI_VK_FRAMES_IN_FLIGHT] = {};
	};
}
