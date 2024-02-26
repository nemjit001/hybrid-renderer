#pragma once

#include <vulkan/vulkan.h>

#include "renderer_internal/render_context.h"

using namespace hri;

namespace hri_debug
{
	/// @brief The debug label handler allows setting debug labels for Vulkan objects.
	class DebugLabelHandler
	{
	public:
		/// @brief Create a new debug label handler.
		/// @param ctx Render Context to use.
		DebugLabelHandler(RenderContext& ctx);

		/// @brief Destroy this Debug Label Handler.
		virtual ~DebugLabelHandler() = default;

		/// @brief Begin a new command buffer label.
		/// @param commandBuffer Command buffer to use.
		/// @param pName Name to give to this command buffer region.
		void cmdBeginLabel(VkCommandBuffer commandBuffer, const char* pName) const;

		/// @brief End a command buffer label region.
		/// @param commandBuffer Command buffer to use.
		void cmdEndLabel(VkCommandBuffer commandBuffer) const;

	protected:
		PFN_vkCmdBeginDebugUtilsLabelEXT vkBeginDebugUtilsLabelEXT;
		PFN_vkCmdEndDebugUtilsLabelEXT vkEndDebugUtilsLabelEXT;
	};
}
