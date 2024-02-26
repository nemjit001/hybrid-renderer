#include "renderer_internal/debug.h"

#include <vulkan/vulkan.h>

#include "renderer_internal/render_context.h"

using namespace hri;
using namespace hri_debug;

DebugLabelHandler::DebugLabelHandler(RenderContext& ctx)
	:
	vkBeginDebugUtilsLabelEXT(ctx.getInstanceFunction<PFN_vkCmdBeginDebugUtilsLabelEXT>("vkCmdBeginDebugUtilsLabelEXT")),
	vkEndDebugUtilsLabelEXT(ctx.getInstanceFunction<PFN_vkCmdEndDebugUtilsLabelEXT>("vkCmdEndDebugUtilsLabelEXT"))
{
	//
}

void DebugLabelHandler::cmdBeginLabel(VkCommandBuffer commandBuffer, const char* pName) const
{
	VkDebugUtilsLabelEXT labelInfo = VkDebugUtilsLabelEXT{ VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
	labelInfo.pLabelName = pName;

	this->vkBeginDebugUtilsLabelEXT(commandBuffer, &labelInfo);
}

void DebugLabelHandler::cmdEndLabel(VkCommandBuffer commandBuffer) const
{
	this->vkEndDebugUtilsLabelEXT(commandBuffer);
}
