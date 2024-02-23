#include "renderer_internal/buffer.h"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "renderer_internal/render_context.h"

using namespace hri;

BufferResource::BufferResource(RenderContext& ctx, size_t size, VkBufferUsageFlags usage, bool hostVisible)
	:
	m_ctx(ctx),
	bufferSize(size),
	hostVisible(hostVisible)
{
	assert(bufferSize > 0);

	VkBufferCreateInfo bufferCreateInfo = VkBufferCreateInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bufferCreateInfo.flags = 0;
	bufferCreateInfo.size = bufferSize;
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

	HRI_VK_CHECK(vmaCreateBuffer(m_ctx.allocator, &bufferCreateInfo, &allocateInfo, &buffer, &m_allocation, nullptr));
}

BufferResource::~BufferResource()
{
	release();
}

BufferResource::BufferResource(BufferResource&& other) noexcept
	:
	m_ctx(other.m_ctx),
	m_allocation(other.m_allocation),
	bufferSize(other.bufferSize),
	hostVisible(other.hostVisible),
	buffer(other.buffer)
{
	other.buffer = VK_NULL_HANDLE;
	other.m_allocation = VK_NULL_HANDLE;
}

BufferResource& BufferResource::operator=(BufferResource&& other) noexcept
{
	if (this == &other)
	{
		return *this;
	}

	release();
	m_ctx = std::move(other.m_ctx);
	m_allocation = other.m_allocation;
	bufferSize = other.bufferSize;
	hostVisible = other.hostVisible;
	buffer = other.buffer;

	other.buffer = VK_NULL_HANDLE;
	other.m_allocation = VK_NULL_HANDLE;

	return *this;
}

void BufferResource::copyToBuffer(const void* pData, size_t size, size_t offset)
{
	assert(pData != nullptr);
	assert(offset + size <= bufferSize);
	assert(hostVisible == true);

	void* pBuffer = nullptr;
	vmaMapMemory(m_ctx.allocator, m_allocation, &pBuffer);
	assert(pBuffer != nullptr);

	void* pBufferLocation = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(pBuffer) + offset);
	memcpy(pBufferLocation, pData, size);

	vmaUnmapMemory(m_ctx.allocator, m_allocation);
}

void BufferResource::release()
{
	vmaDestroyBuffer(m_ctx.allocator, buffer, m_allocation);
}
