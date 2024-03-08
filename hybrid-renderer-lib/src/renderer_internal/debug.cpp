#include "renderer_internal/debug.h"

#include <vulkan/vulkan.h>

#include "renderer_internal/render_context.h"

using namespace hri;
using namespace hri_debug;

DebugLabelHandler::DebugLabelHandler(RenderContext& ctx)
	:
	m_ctx(ctx),
	vkBeginDebugUtilsLabelEXT(ctx.getInstanceFunction<PFN_vkCmdBeginDebugUtilsLabelEXT>("vkCmdBeginDebugUtilsLabelEXT")),
	vkEndDebugUtilsLabelEXT(ctx.getInstanceFunction<PFN_vkCmdEndDebugUtilsLabelEXT>("vkCmdEndDebugUtilsLabelEXT"))
{
	VkPhysicalDeviceProperties props = {};
	vkGetPhysicalDeviceProperties(m_ctx.gpu, &props);
	m_timestampPeriod = props.limits.timestampPeriod / 1'000'000.0f;	// nanoseconds to milliseconds

	VkQueryPoolCreateInfo qpCreateInfo = VkQueryPoolCreateInfo{ VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
	qpCreateInfo.flags = 0;
	qpCreateInfo.pipelineStatistics = 0; // unused for timestamps
	qpCreateInfo.queryCount = 2;
	qpCreateInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;

	HRI_VK_CHECK(vkCreateQueryPool(m_ctx.device, &qpCreateInfo, nullptr, &m_timerPool));
	vkResetQueryPool(m_ctx.device, m_timerPool, 0, 2);
}

DebugLabelHandler::~DebugLabelHandler()
{
	vkDestroyQueryPool(m_ctx.device, m_timerPool, nullptr);
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

void DebugLabelHandler::cmdRecordStartTimestamp(VkCommandBuffer commandBuffer) const
{
	vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_timerPool, 0);
}

void DebugLabelHandler::cmdRecordEndTimestamp(VkCommandBuffer commandBuffer) const
{
	vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_timerPool, 1);
}

void DebugLabelHandler::resetTimer() const
{
	vkResetQueryPool(m_ctx.device, m_timerPool, 0, 2);
}

float DebugLabelHandler::timeDelta() const
{
	uint64_t timestamps[2] = { 0, 0 };
	VkResult result = vkGetQueryPoolResults(m_ctx.device, m_timerPool, 0, 2, sizeof(timestamps), timestamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
	float timeDelta = 0.0;

	if (result == VK_NOT_READY)
	{
		return timeDelta;
	}
	else if (result == VK_SUCCESS)
	{
		uint64_t delta = timestamps[1] - timestamps[0];
		timeDelta = static_cast<float>(delta) * m_timestampPeriod;
	}

	vkResetQueryPool(m_ctx.device, m_timerPool, 0, 2);
	return timeDelta;
}
