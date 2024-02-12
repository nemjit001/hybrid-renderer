#include "renderer_internal/render_core.h"

#include "config.h"
#include "platform.h"
#include "renderer_internal/render_context.h"

using namespace hri;

FrameState FrameState::init(RenderContext* ctx)
{
	assert(ctx != nullptr);
	FrameState frameState = FrameState{};

	VkFenceCreateInfo frameReadyFenceCreateInfo = VkFenceCreateInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	frameReadyFenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	HRI_VK_CHECK(vkCreateFence(ctx->device, &frameReadyFenceCreateInfo, nullptr, &frameState.frameReady));

	VkSemaphoreCreateInfo semaphoreCreateInfo = VkSemaphoreCreateInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	semaphoreCreateInfo.flags = 0;
	HRI_VK_CHECK(vkCreateSemaphore(ctx->device, &semaphoreCreateInfo, nullptr, &frameState.imageAvailable));
	HRI_VK_CHECK(vkCreateSemaphore(ctx->device, &semaphoreCreateInfo, nullptr, &frameState.renderingFinished));

	VkCommandPoolCreateInfo gfxPoolCreateInfo = VkCommandPoolCreateInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	gfxPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	gfxPoolCreateInfo.queueFamilyIndex = ctx->queues.graphicsQueue.family;
	HRI_VK_CHECK(vkCreateCommandPool(ctx->device, &gfxPoolCreateInfo, nullptr, &frameState.graphicsCommandPool));

	VkCommandBufferAllocateInfo gfxBufferAllocateInfo = VkCommandBufferAllocateInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	gfxBufferAllocateInfo.commandPool = frameState.graphicsCommandPool;
	gfxBufferAllocateInfo.commandBufferCount = 1;
	gfxBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	HRI_VK_CHECK(vkAllocateCommandBuffers(ctx->device, &gfxBufferAllocateInfo, &frameState.graphicsCommandBuffer));

	return frameState;
}

void FrameState::destroy(RenderContext* ctx, FrameState& frameState)
{
	assert(ctx != nullptr);
	HRI_VK_CHECK(vkWaitForFences(ctx->device, 1, &frameState.frameReady, VK_TRUE, UINT64_MAX));

	vkDestroyFence(ctx->device, frameState.frameReady, nullptr);
	vkDestroySemaphore(ctx->device, frameState.imageAvailable, nullptr);
	vkDestroySemaphore(ctx->device, frameState.renderingFinished, nullptr);
	vkDestroyCommandPool(ctx->device, frameState.graphicsCommandPool, nullptr);

	memset(&frameState, 0, sizeof(FrameState));
}

RenderCore::RenderCore(RenderContext* ctx)
	:
	m_pCtx(ctx)
{
	assert(ctx != nullptr);

	VkCommandPoolCreateInfo submitPoolCreateInfo = VkCommandPoolCreateInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	submitPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	submitPoolCreateInfo.queueFamilyIndex = m_pCtx->queues.graphicsQueue.family;
	HRI_VK_CHECK(vkCreateCommandPool(m_pCtx->device, &submitPoolCreateInfo, nullptr, &m_submitPool));

	// Set up per frame state
	for (size_t i = 0; i < HRI_VK_FRAMES_IN_FLIGHT; i++)
	{
		m_frames[i] = FrameState::init(m_pCtx);
	}
}

RenderCore::~RenderCore()
{
	// Destroy vulkan resources
	vkDestroyCommandPool(m_pCtx->device, m_submitPool, nullptr);

	// Destroy per frame state
	for (size_t i = 0; i < HRI_VK_FRAMES_IN_FLIGHT; i++)
	{
		FrameState::destroy(m_pCtx, m_frames[i]);
	}
}

void RenderCore::startFrame()
{
	FrameState& activeFrame = m_frames[m_currentFrame];

	VkFence frameFences[] = { activeFrame.frameReady };
	HRI_VK_CHECK(vkWaitForFences(m_pCtx->device, HRI_SIZEOF_ARRAY(frameFences), frameFences, VK_TRUE, UINT64_MAX));

	if (m_recreateSwapchain)
	{
		m_pCtx->recreateSwapchain();

		if (m_onSwapchainInvalidateFunc != nullptr)
			m_onSwapchainInvalidateFunc(m_pCtx->swapchain);

		m_recreateSwapchain = false;
	}

	validateSwapchainState(vkAcquireNextImageKHR(m_pCtx->device, m_pCtx->swapchain, UINT64_MAX, activeFrame.imageAvailable, VK_NULL_HANDLE, &m_activeSwapImage));
}

void RenderCore::endFrame()
{
	FrameState& activeFrame = m_frames[m_currentFrame];

	// Can't do work if the swapchain needs to be recreated
	if (m_recreateSwapchain)
		return;

	VkFence frameFences[] = { activeFrame.frameReady };
	HRI_VK_CHECK(vkResetFences(m_pCtx->device, HRI_SIZEOF_ARRAY(frameFences), frameFences));

	VkPipelineStageFlags waitStages[] = {
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
	};

	VkSubmitInfo frameSubmit = VkSubmitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
	frameSubmit.commandBufferCount = 1;
	frameSubmit.pCommandBuffers = &activeFrame.graphicsCommandBuffer;
	frameSubmit.waitSemaphoreCount = 1;
	frameSubmit.pWaitSemaphores = &activeFrame.imageAvailable;
	frameSubmit.pWaitDstStageMask = waitStages;
	frameSubmit.signalSemaphoreCount = 1;
	frameSubmit.pSignalSemaphores = &activeFrame.renderingFinished;
	HRI_VK_CHECK(vkQueueSubmit(m_pCtx->queues.graphicsQueue.handle, 1, &frameSubmit, activeFrame.frameReady));

	VkSwapchainKHR swapchains[] = { m_pCtx->swapchain };
	uint32_t imageIndices[] = { m_activeSwapImage };
	assert(HRI_SIZEOF_ARRAY(swapchains) == HRI_SIZEOF_ARRAY(imageIndices));

	VkPresentInfoKHR presentInfo = VkPresentInfoKHR{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
	presentInfo.swapchainCount = HRI_SIZEOF_ARRAY(swapchains);
	presentInfo.pSwapchains = swapchains;
	presentInfo.pImageIndices = imageIndices;
	presentInfo.pResults = nullptr;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &activeFrame.renderingFinished;
	validateSwapchainState(vkQueuePresentKHR(m_pCtx->queues.presentQueue.handle, &presentInfo));

	m_previousFrame = m_currentFrame;
	m_currentFrame = (m_currentFrame + 1) % HRI_VK_FRAMES_IN_FLIGHT;
}

void RenderCore::awaitFrameFinished() const
{
	assert(HRI_VK_FRAMES_IN_FLIGHT == 1 || m_previousFrame != m_currentFrame);

	const FrameState& activeFrame = m_frames[m_previousFrame];
	VkFence frameFences[] = { activeFrame.frameReady };
	HRI_VK_CHECK(vkWaitForFences(m_pCtx->device, HRI_SIZEOF_ARRAY(frameFences), frameFences, VK_TRUE, UINT64_MAX));
}

HRIOnSwapchainInvalidateFunc RenderCore::setOnSwapchainInvalidateCallback(HRIOnSwapchainInvalidateFunc onSwapchainInvalidate)
{
	HRIOnSwapchainInvalidateFunc old = m_onSwapchainInvalidateFunc;
	m_onSwapchainInvalidateFunc = onSwapchainInvalidate;

	return old;
}

void RenderCore::immediateSubmit(HRIImmediateSubmitFunc submitFunc)
{
	VkCommandBuffer oneshotBuffer = VK_NULL_HANDLE;

	VkCommandBufferAllocateInfo oneshotAllocateInfo = VkCommandBufferAllocateInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	oneshotAllocateInfo.commandPool = m_submitPool;
	oneshotAllocateInfo.commandBufferCount = 1;
	oneshotAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	HRI_VK_CHECK(vkAllocateCommandBuffers(m_pCtx->device, &oneshotAllocateInfo, &oneshotBuffer));

	VkCommandBufferBeginInfo beginOneshot = VkCommandBufferBeginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	beginOneshot.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	beginOneshot.pInheritanceInfo = nullptr;
	vkBeginCommandBuffer(oneshotBuffer, &beginOneshot);
	submitFunc(oneshotBuffer);
	vkEndCommandBuffer(oneshotBuffer);

	VkSubmitInfo oneshotSubmit = VkSubmitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
	oneshotSubmit.commandBufferCount = 1;
	oneshotSubmit.pCommandBuffers = &oneshotBuffer;
	// FIXME: Add fence based synchronization & use explicit queue for operation type!
	HRI_VK_CHECK(vkQueueSubmit(m_pCtx->queues.graphicsQueue.handle, 1, &oneshotSubmit, VK_NULL_HANDLE));
	vkDeviceWaitIdle(m_pCtx->device);

	vkFreeCommandBuffers(m_pCtx->device, m_submitPool, 1, &oneshotBuffer);
}

void RenderCore::recordFrameGraph(FrameGraph& frameGraph)
{
	FrameState& activeFrame = m_frames[m_currentFrame];

	HRI_VK_CHECK(vkResetCommandBuffer(activeFrame.graphicsCommandBuffer, 0 /* No reset flags */));

	VkCommandBufferBeginInfo frameBeginInfo = VkCommandBufferBeginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	frameBeginInfo.flags = 0;
	frameBeginInfo.pInheritanceInfo = nullptr;
	HRI_VK_CHECK(vkBeginCommandBuffer(activeFrame.graphicsCommandBuffer, &frameBeginInfo));

	frameGraph.execute(activeFrame.graphicsCommandBuffer, m_activeSwapImage);

	HRI_VK_CHECK(vkEndCommandBuffer(activeFrame.graphicsCommandBuffer));
}

void RenderCore::validateSwapchainState(VkResult result)
{
	if (result == VK_SUCCESS)
		return;

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
	{
		m_recreateSwapchain = true;
	}
	else
	{
		abort(); // Fatal error, probably always abort?
	}
}
