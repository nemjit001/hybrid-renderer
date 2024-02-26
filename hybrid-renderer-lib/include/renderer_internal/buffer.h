#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "renderer_internal/render_context.h"

namespace hri
{
	/// @brief A Buffer Resource is a collection of a vk buffer and its allocation.
	class BufferResource
	{
	public:
		/// @brief Create a new Buffer Resource.
		/// @param ctx Render Context to use.
		/// @param size Size of the buffer to be created, cannot be 0.
		/// @param usage Intended buffer usage.
		/// @param hostVisible If the buffer is host visible and can be mapped to memory.
		BufferResource(RenderContext& ctx, size_t size, VkBufferUsageFlags usage, bool hostVisible = false);

		/// @brief Destroy this Buffer Resource.
		virtual ~BufferResource();

		// Disallow copy behaviour
		BufferResource(const BufferResource&) = delete;
		BufferResource& operator=(const BufferResource&) = delete;

		// Allow move semantics
		BufferResource(BufferResource&& other) noexcept;
		BufferResource& operator=(BufferResource&& other) noexcept;

		/// @brief Copy data to this buffer.
		/// @param pData Data to copy.
		/// @param size Size of the data to copy.
		/// @param offset Offset into target buffer for data start.
		void copyToBuffer(const void* pData, size_t size, size_t offset = 0);

		void* map() const;

		void unmap() const;

		/// @brief Get Buffer Device Address info for this buffer.
		/// @return A filled out VkBufferDeviceAddressInfo struct.
		inline VkBufferDeviceAddressInfo deviceAddressInfo() const
		{
			VkBufferDeviceAddressInfo addressInfo = VkBufferDeviceAddressInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
			addressInfo.buffer = buffer;
			return addressInfo;
		};

	private:
		void release();

	public:
		size_t bufferSize			= 0;
		bool hostVisible			= false;
		VkBuffer buffer				= VK_NULL_HANDLE;

	private:
		RenderContext& m_ctx;
		VmaAllocation m_allocation	= VK_NULL_HANDLE;
	};
}
