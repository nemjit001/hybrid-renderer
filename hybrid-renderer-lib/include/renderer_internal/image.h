#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "renderer_internal/render_context.h"

namespace hri
{
	/// @brief An image resource is a collection of a vk image, allocation, and optional view.
    ///     The view can be instantiated after creation and is destroyed on image resource destruction.
	struct ImageResource
	{
        VkExtent3D extent               = VkExtent3D{};
		VkFormat format				    = VK_FORMAT_UNDEFINED;
		VkImage image				    = VK_NULL_HANDLE;
        VkImageView view                = VK_NULL_HANDLE;
		VmaAllocation allocation	    = VK_NULL_HANDLE;

		/// @brief Initialize a new image resource.
		/// @param ctx Render Context to use.
		/// @param type Image type.
		/// @param format Image format.
		/// @param samples Image MSAA samples.
		/// @param extent Image extent in 3D.
		/// @param mipLevels Image mip levels.
		/// @param arrayLayers Image array layers.
		/// @param usage Image usage flags.
		/// @param tiling Image tiling mode.
		/// @param flags Image create flags.
		/// @param initialLayout Initial image layout.
		/// @return An initialized image resource.
		static ImageResource init(
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

		/// @brief Destroy an image resource.
		/// @param ctx Render Context used to create image.
		/// @param image Image to destroy.
		static void destroy(RenderContext* ctx, ImageResource& image);

		/// @brief Create a new internal image view and return its handle.
		/// @param ctx Render Context to use.
		/// @param viewType View type.
		/// @param components Color component mapping.
		/// @param subresourceRange Image subresource range for view.
		/// @return A new image view handle.
		VkImageView createView(RenderContext* ctx, VkImageViewType viewType, VkComponentMapping components, VkImageSubresourceRange subresourceRange);

		/// @brief Destroy the internal image view handle.
		/// @param ctx Render context used to create the view.
		void destroyView(RenderContext* ctx);
	};
}
