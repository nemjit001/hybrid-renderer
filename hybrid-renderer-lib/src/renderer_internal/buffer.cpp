#include "renderer_internal/buffer.h"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "renderer_internal/render_context.h"

using namespace hri;

BufferResource BufferResource::init(RenderContext* ctx, size_t size, VkBufferUsageFlags usage, bool hostVisible)
{
	assert(ctx != nullptr);
	assert(size > 0);

	BufferResource buffer = BufferResource{};
	buffer.bufferSize = size;
	buffer.hostVisible = true;

	VkBufferCreateInfo bufferCreateInfo = VkBufferCreateInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bufferCreateInfo.flags = 0;
	bufferCreateInfo.size = size;
	bufferCreateInfo.usage = usage;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bufferCreateInfo.queueFamilyIndexCount = 0;
	bufferCreateInfo.pQueueFamilyIndices = nullptr;

	VmaAllocationCreateInfo allocateInfo = VmaAllocationCreateInfo{};
	allocateInfo.usage = VMA_MEMORY_USAGE_AUTO;
	allocateInfo.flags = 0;

	if (hostVisible)
	{
		allocateInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	}

	HRI_VK_CHECK(vmaCreateBuffer(ctx->allocator, &bufferCreateInfo, &allocateInfo, &buffer.buffer, &buffer.allocation, nullptr));
	return buffer;
}

void BufferResource::destroy(RenderContext* ctx, BufferResource& buffer)
{
	assert(ctx != nullptr);

	vmaDestroyBuffer(ctx->allocator, buffer.buffer, buffer.allocation);

	memset(&buffer, 0, sizeof(BufferResource));
}

void BufferResource::copyToBuffer(RenderContext* ctx, const void* pData, size_t size, size_t offset)
{
	assert(ctx != nullptr);
	assert(pData != nullptr);
	assert(offset + size <= bufferSize);
	assert(hostVisible);

	void* pBuffer = nullptr;
	vmaMapMemory(ctx->allocator, allocation, &pBuffer);
	assert(pBuffer != nullptr);

	void* pBufferLocation = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(pBuffer) + offset);
	memcpy(pBufferLocation, pData, size);

	vmaUnmapMemory(ctx->allocator, allocation);
}
