#include "renderer_internal/command_submission.h"

#include <vulkan/vulkan.h>

#include "renderer_internal/render_context.h"

using namespace hri;

CommandPool::CommandPool(RenderContext& ctx, DeviceQueue queue, VkCommandPoolCreateFlags flags)
	:
	m_ctx(ctx),
	m_queue(queue)
{
	VkFenceCreateInfo fenceCreateInfo = VkFenceCreateInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	fenceCreateInfo.flags = 0;
	HRI_VK_CHECK(vkCreateFence(m_ctx.device, &fenceCreateInfo, nullptr, &m_submitFence));

	VkCommandPoolCreateInfo createInfo = VkCommandPoolCreateInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	createInfo.flags = flags | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	createInfo.queueFamilyIndex = queue.family;
	HRI_VK_CHECK(vkCreateCommandPool(m_ctx.device, &createInfo, nullptr, &m_commandPool));
}

CommandPool::~CommandPool()
{
	release();
}

VkCommandBuffer CommandPool::createCommandBuffer(VkCommandBufferUsageFlags usage, bool begin) const
{
	VkCommandBufferAllocateInfo allocateInfo = VkCommandBufferAllocateInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	allocateInfo.commandBufferCount = 1;
	allocateInfo.commandPool = m_commandPool;
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
	HRI_VK_CHECK(vkAllocateCommandBuffers(m_ctx.device, &allocateInfo, &commandBuffer));

	if (begin)
	{
		VkCommandBufferBeginInfo beginInfo = VkCommandBufferBeginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		beginInfo.flags = usage;
		beginInfo.pInheritanceInfo = nullptr;
		HRI_VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));
	}

	return commandBuffer;
}

void CommandPool::freeCommandBuffer(VkCommandBuffer cmdBuffer) const
{
	vkFreeCommandBuffers(m_ctx.device, m_commandPool, 1, &cmdBuffer);
	cmdBuffer = VK_NULL_HANDLE;
}

void CommandPool::resetCommandBuffer(VkCommandBuffer cmdBuffer, VkCommandBufferResetFlags flags) const
{
	HRI_VK_CHECK(vkResetCommandBuffer(cmdBuffer, flags));
}

void CommandPool::reset(VkCommandPoolResetFlags flags)
{
	HRI_VK_CHECK(vkResetCommandPool(m_ctx.device, m_commandPool, flags));
}

void CommandPool::submitAndWait(VkCommandBuffer cmdBuffer, bool endRecording)
{
	HRI_VK_CHECK(vkResetFences(m_ctx.device, 1, &m_submitFence));

	if (endRecording)
		HRI_VK_CHECK(vkEndCommandBuffer(cmdBuffer));

	VkSubmitInfo submitInfo = VkSubmitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmdBuffer;

	HRI_VK_CHECK(vkQueueSubmit(m_queue.handle, 1, &submitInfo, m_submitFence));
	HRI_VK_CHECK(vkWaitForFences(m_ctx.device, 1, &m_submitFence, VK_TRUE, UINT64_MAX));
}

void CommandPool::release()
{
	reset();
	vkDestroyFence(m_ctx.device, m_submitFence, nullptr);
	vkDestroyCommandPool(m_ctx.device, m_commandPool, nullptr);
}
