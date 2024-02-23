#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "renderer_internal/render_context.h"

namespace hri
{
	/// @brief An image resource is a collection of a vk image, allocation, and optional view.
    ///     The view can be instantiated after creation and is destroyed on image resource destruction.
	class ImageResource
	{
	public:
		/// @brief Initialize a new Image Resource.
		/// @param ctx 
		/// @param type 
		/// @param format 
		/// @param samples 
		/// @param extent 
		/// @param mipLevels 
		/// @param arrayLayers 
		/// @param usage 
		/// @param tiling 
		/// @param flags 
		/// @param initialLayout 
		ImageResource(
			RenderContext* ctx,
			VkImageType type,
			VkFormat format,
			VkSampleCountFlagBits samples,
			VkExtent3D extent,
			uint32_t mipLevels,
			uint32_t arrayLayers,
			VkImageUsageFlags usage,
			VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL,
			VkImageCreateFlags flags = 0,
			VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
		);

		/// @brief Destroy this image resource.
		virtual ~ImageResource();

		// Disallow copy behaviour
		ImageResource(const ImageResource&) = delete;
		ImageResource& operator=(const ImageResource&) = delete;

		// Allow move semantics
		ImageResource(ImageResource&& other) noexcept;
		ImageResource& operator=(ImageResource&& other) noexcept;

		/// @brief Create a new internal image view and return its handle.
		/// @param viewType View type.
		/// @param components Color component mapping.
		/// @param subresourceRange Image subresource range for view.
		/// @return A new image view handle.
		VkImageView createView(VkImageViewType viewType, VkComponentMapping components, VkImageSubresourceRange subresourceRange);

		/// @brief Destroy the internal image view handle.
		void destroyView();

	private:
		void release();

	public:
        VkExtent3D extent               = VkExtent3D{};
		VkFormat format				    = VK_FORMAT_UNDEFINED;
		VkImage image				    = VK_NULL_HANDLE;
        VkImageView view                = VK_NULL_HANDLE;

	private:
		RenderContext* m_pCtx			= nullptr;
		VmaAllocation m_allocation	    = VK_NULL_HANDLE;
	};
}
