#pragma once

#include <optional>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "renderer_internal/image.h"
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

	/// @brief The Render Attachment Configuration is used by Render Pass resource managers to create and recreate
	///		the required pass attachments.
	struct RenderAttachmentConfig
	{
		VkFormat format;
		VkSampleCountFlagBits samples;
		VkImageUsageFlags usage;
		VkImageAspectFlags aspect;
	};

	/// @brief The Render Pass Resource Manager Base is used to create and recreate render pass resources.
	///		It also handles render pass start & finish, and clear value registration.
	class IRenderPassResourceManagerBase
	{
	public:
		/// @brief Create a new render pass resource manager base.
		/// @param ctx Render context to use.
		/// @param renderPass Render Pass for which to manage resources.
		/// @param attachmentConfigs Attachment configurations.
		IRenderPassResourceManagerBase(
			RenderContext* ctx,
			VkRenderPass renderPass,
			const std::vector<RenderAttachmentConfig>& attachmentConfigs
		);

		/// @brief Destroy this resource manager base.
		virtual ~IRenderPassResourceManagerBase();

		// Disallow copy behaviour
		IRenderPassResourceManagerBase(IRenderPassResourceManagerBase&) = delete;
		IRenderPassResourceManagerBase& operator=(IRenderPassResourceManagerBase&) = delete;

		/// @brief Begin a new render pass (override in child classes).
		/// @param frame Active Frame for which to record commands.
		virtual void beginRenderPass(ActiveFrame& frame) const = 0;

		/// @brief End render pass.
		/// @param frame Active Frame for which to record commands.
		virtual void endRenderPass(ActiveFrame& frame) const;

		/// @brief Set an attachment clear value.
		/// @param attachmentIndex Attachment for which to set clear value.
		/// @param clearValue Clear value to set.
		virtual void setClearValue(uint32_t attachmentIndex, VkClearValue clearValue);

		/// @brief Retrieve the managed render pass.
		/// @return A vk render pass handle.
		inline virtual VkRenderPass renderPass() const { return m_renderPass; }

		/// @brief Get a managed attachment resource.
		/// @param attachmentIndex Attachment resource index to retrieve.
		/// @return An ImageResource managed by this resource manager.
		inline virtual const ImageResource& getAttachmentResource(uint32_t attachmentIndex) const {
			assert(attachmentIndex < m_imageResources.size());
			return m_imageResources[attachmentIndex];
		}

		/// @brief Recreate resources.
		inline virtual void recreateResources() {
			destroyResources();
			createResources();
		}

	protected:
		/// @brief Create resources.
		virtual void createResources();

		/// @brief Destroy resources.
		virtual void destroyResources();

		/// @brief Get image resource views.
		/// @return A vector of image views.
		virtual std::vector<VkImageView> getImageResourceViews();

		/// @brief Get the render extent configured for this resouce manager.
		/// @return The render extent.
		virtual VkExtent2D getRenderExtent() const;

	protected:
		RenderContext* m_pCtx = nullptr;
		VkRenderPass m_renderPass = VK_NULL_HANDLE;
		VkExtent2D m_renderExtent = VkExtent2D{};
		std::vector<VkClearValue> m_clearValues = {};
		std::vector<RenderAttachmentConfig> m_attachmentConfigs = {};
		std::vector<ImageResource> m_imageResources = {};
	};

	/// @brief The Swapchain pass resource manager is a special type of resource manager that handles rendering
	///		to swap resources.
	class SwapchainPassResourceManager
		:
		public IRenderPassResourceManagerBase
	{
	public:
		/// @brief Create a new Swapchain resource manager.
		/// @param ctx Render Context to use.
		/// @param renderPass Render pass for which to manage resources.
		/// @param attachmentConfigs Pass attachment configs.
		SwapchainPassResourceManager(
			RenderContext* ctx,
			VkRenderPass renderPass,
			const std::vector<RenderAttachmentConfig>& attachmentConfigs = {}
		);

		/// @brief Destroy this swapchain resource manager.
		virtual ~SwapchainPassResourceManager();

		/// @brief Begin a render pass rendering to the swapchain.
		/// @param frame Active Frame to record into.
		virtual void beginRenderPass(ActiveFrame& frame) const override;

	protected:
		/// @brief Create resources.
		virtual void createResources() override;

		/// @brief Destroy resources.
		virtual void destroyResources() override;

	private:
		std::vector<VkImageView> m_swapViews = {};
		std::vector<VkFramebuffer> m_swapFramebuffers = {};
	};

	/// @brief The Render Pass Resource manager handles drawing to a framebuffer constructed using the interface's
	///		configuration data.
	class RenderPassResourceManager
		:
		public IRenderPassResourceManagerBase
	{
	public:
		/// @brief Create a new Render Pass Resource Manager.
		/// @param ctx Render Context to use.
		/// @param renderPass Render pass to manage resources for.
		/// @param attachmentConfigs Pass attachment configs.
		RenderPassResourceManager(
			RenderContext* ctx,
			VkRenderPass renderPass,
			const std::vector<RenderAttachmentConfig>& attachmentConfigs = {}
		);

		/// @brief Destroy this resource manager.
		virtual ~RenderPassResourceManager();

		/// @brief Begin a new render pass, rendering into an offscreen framebuffer.
		/// @param frame Active Frame to record into.
		virtual void beginRenderPass(ActiveFrame& frame) const override;

	protected:
		/// @brief Create resources.
		virtual void createResources() override;

		/// @brief Destroy resources.
		virtual void destroyResources() override;

	private:
		VkFramebuffer m_framebuffer = VK_NULL_HANDLE;
	};
}
