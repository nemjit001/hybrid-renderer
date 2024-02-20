#pragma once

#include <optional>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "renderer_internal/render_core.h"
#include "renderer_internal/shader_database.h"

namespace hri
{
	/// @brief Render pass attachment type.
	enum class AttachmentType
	{
		Color,
		DepthStencil,
	};

	/// @brief The RenderPassBuilder allows for easy setup of render passes.
	class RenderPassBuilder
	{
	public:
		/// @brief Create a new render pass builder.
		/// @param ctx Render context to use.
		RenderPassBuilder(RenderContext* ctx = nullptr);

		/// @brief Destroy this render pass builder.
		virtual ~RenderPassBuilder() = default;

		/// @brief Add a new attachment to the render pass.
		/// @param format 
		/// @param samples 
		/// @param finalLayout 
		/// @param loadOp 
		/// @param storeOp 
		/// @param stencilLoadOp 
		/// @param stencilStoreOp 
		/// @param initialLayout 
		/// @return RenderPassBuilder reference.
		RenderPassBuilder& addAttachment(
			VkFormat format,
			VkSampleCountFlagBits samples,
			VkImageLayout finalLayout,
			VkAttachmentLoadOp loadOp,
			VkAttachmentStoreOp storeOp,
			VkAttachmentLoadOp stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VkAttachmentStoreOp stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
		);

		/// @brief Start a new subpass.
		/// @return RenderPassBuilder reference.
		RenderPassBuilder& nextSubpass();

		/// @brief Add an attachment reference to the current subpass.
		/// @param type Type of reference to add.
		/// @param ref Reference.
		/// @return RenderPassBuilder reference.
		RenderPassBuilder& setAttachmentReference(AttachmentType type, VkAttachmentReference ref);

		/// @brief Update the used render context.
		/// @param ctx New render context to use for render pass creation.
		/// @return RenderPassBuilder reference.
		RenderPassBuilder& setContext(RenderContext* ctx);

		/// @brief Build the configured render pass.
		/// @return RenderPassBuilder reference.
		VkRenderPass build();

	private:
		struct SubpassData
		{
			std::vector<VkAttachmentReference> colorAttachments;
			std::optional<VkAttachmentReference> depthStencilAttachment;
		};

		RenderContext* m_pCtx = nullptr;
		std::vector<VkAttachmentDescription> m_attachments;
		std::vector<SubpassData> m_subpasses;
	};
}
