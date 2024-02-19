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

	struct RenderAttachment	// XXX: More like image -> move to separate header?
	{
		VkFormat format				= VK_FORMAT_UNDEFINED;
		VkImage image				= VK_NULL_HANDLE;
		VmaAllocation allocation	= VK_NULL_HANDLE;

		static RenderAttachment init(RenderContext* ctx, VkImageCreateInfo* createInfo);

		static void destroy(RenderContext* ctx, RenderAttachment& attachment);

		VkImageView createView(RenderContext* ctx, VkImageViewType viewType, VkComponentMapping components, VkImageSubresourceRange subresourceRange);

		void destroyView(RenderContext* ctx, VkImageView view);
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

	/// @brief The Recordable Pass Interface is used to as a unifying interface for pass recording.
	class IRecordablePass
	{
	public:
		IRecordablePass(RenderContext* ctx) : m_pCtx(ctx) { assert(m_pCtx != nullptr); }

		virtual ~IRecordablePass() = default;

		virtual void createFrameResources() = 0;

		virtual void destroyFrameResources() = 0;

		inline virtual void recreateFrameResources()
		{
			destroyFrameResources();
			createFrameResources();
		}

		virtual void setInputAttachment(uint32_t binding, const VkImageView& attachment) = 0;

		virtual std::vector<VkImageView> getOutputAttachments() const = 0;

		/// @brief Record a pass into an active frame's graphics command buffer.
		/// @param frame Frame to record pass into.
		virtual void record(const ActiveFrame& frame) const = 0;

	protected:
		RenderContext* m_pCtx = nullptr;
	};

	/// @brief The Builtin Render Pass is a simple present pass that draw a fullscreen triangle to
	///		a swap image. This pass takes in *1* image sampler as input, drawing its image contents every frame.
	class BuiltinRenderPass
	{
	public:
		/// @brief Create an empty builtin pass, will be invalid.
		BuiltinRenderPass() = default;

		/// @brief Create a new builtin pass.
		/// @param ctx Render Context to use.
		/// @param shaderDB Shader Database to use, will register builtin shaders with database.
		BuiltinRenderPass(RenderContext* ctx, ShaderDatabase* shaderDB);

		/// @brief Destroy this builtin pass.
		virtual ~BuiltinRenderPass();

		/// @brief Create pass resources needed for rendering.
		void createResources();

		/// @brief  Destroy pass resources.
		void destroyResources();

		/// @brief Execute the builtin pass.
		/// @param commandBuffer Command buffer to record into.
		/// @param activeSwapImage Swap image index to draw into.
		void execute(VkCommandBuffer commandBuffer, uint32_t activeSwapImage) const;

	private:
		/// @brief The Pipeline Layout Description is fixed for the Builtin pass.
		/// @return The layout description.
		PipelineLayoutDescription builtinPipelineLayoutDescription() const;

		/// @brief The Graphics Pipeline Builder is fixed for the Builtin pass.
		/// @return The pipeline builder.
		GraphicsPipelineBuilder builtinPipelineBuilder() const;

	private:
		RenderContext* m_pCtx						= nullptr;
		const PipelineStateObject* m_pBuiltinPSO	= nullptr;
		std::vector<VkImageView> m_swapViews		= {};
		std::vector<VkFramebuffer> m_framebuffers	= {};
		VkRenderPass m_renderPass					= VK_NULL_HANDLE;
	};
}
