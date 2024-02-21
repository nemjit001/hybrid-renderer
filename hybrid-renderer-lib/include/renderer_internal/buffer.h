#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "renderer_internal/render_context.h"

namespace hri
{
	/// @brief A Buffer Resource is a collection of a vk buffer and its allocation.
	struct BufferResource
	{
		size_t bufferSize			= 0;
		bool hostVisible			= false;
		VkBuffer buffer				= VK_NULL_HANDLE;
		VmaAllocation allocation	= VK_NULL_HANDLE;

		/// @brief Create a new Buffer Resource.
		/// @param ctx Render Context to use.
		/// @param size Size of the buffer to be created, cannot be 0.
		/// @param usage Intended buffer usage.
		/// @param hostVisible If the buffer is host visible and can be mapped to memory.
		/// @return A new Buffer Resource.
		static BufferResource init(RenderContext* ctx, size_t size, VkBufferUsageFlags usage, bool hostVisible = false);

		/// @brief Destroy a Buffer Resource.
		/// @param ctx Render Context used to create the resource.
		/// @param buffer Buffer to be destroyed.
		static void destroy(RenderContext* ctx, BufferResource& buffer);

		void copyToBuffer(RenderContext* ctx, const void* pData, size_t size, size_t offset = 0);
	};
}
