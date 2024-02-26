#pragma once

#include <vulkan/vulkan.h>

#include "renderer_internal/render_context.h"

namespace hri
{
	/// @brief A command pool handles command submission & command buffer creation.
	class CommandPool
	{
	public:
		/// @brief Create a new command pool.
		/// @param ctx Render Context to use.
		/// @param queue Queue to which this pool will submit.
		/// @param flags Create flags for this pool.
		CommandPool(RenderContext& ctx, DeviceQueue queue, VkCommandPoolCreateFlags flags = 0);

		/// @brief Destroy this command pool.
		virtual ~CommandPool();

		// Disallow copy behaviour
		CommandPool(const CommandPool&) = delete;
		CommandPool& operator=(const CommandPool&) = delete;

		/// @brief Create a new command buffer.
		/// @param usage Usage flags.
		/// @param begin Set to true to automatically begin recording.
		/// @return A new command buffer.
		VkCommandBuffer createCommandBuffer(VkCommandBufferUsageFlags usage = 0, bool begin = true) const;

		/// @brief Free a command buffer.
		/// @param cmdBuffer Command buffer to free.
		void freeCommandBuffer(VkCommandBuffer cmdBuffer) const;

		/// @brief Reset a command buffer.
		/// @param cmdBuffer Command buffer to reset.
		/// @param flags Reset flags.
		void resetCommandBuffer(VkCommandBuffer cmdBuffer, VkCommandBufferResetFlags flags = 0) const;

		/// @brief Reset this command pool.
		/// @param flags Reset flags.
		void reset(VkCommandPoolResetFlags flags = 0);

		/// @brief Submit a recorded command buffer & wait on completion.
		/// @param cmdBuffer Command buffer to submit.
		/// @param endRecording Set to true to end command buffer recording automatically.
		void submitAndWait(VkCommandBuffer cmdBuffer, bool endRecording = true);

	private:
		/// @brief Release resources held by this command pool.
		void release();

	private:
		RenderContext& m_ctx;
		DeviceQueue m_queue			= DeviceQueue{};
		VkCommandPool m_commandPool	= VK_NULL_HANDLE;
	};
}
