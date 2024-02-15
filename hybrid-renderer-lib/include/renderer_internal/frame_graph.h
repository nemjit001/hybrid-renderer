#pragma once

#include <cassert>
#include <map>
#include <optional>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "renderer_internal/render_context.h"
#include "render_pass.h"
#include "renderer_internal/shader_database.h"

namespace hri
{
	/// @brief Forward declaration for FrameGraph class
	class FrameGraph;

	/// @brief Resource Type indicates a Virtual Resource's type.
	enum class ResourceType
	{
		Texture,
	};

	/// @brief A Virtual Resource Handle represents a resource in use by the frame graph.
	///		It is updated using graph nodes, allowing for high level read / write dependency management.
	struct VirtualResourceHandle
	{
		std::string name;
		ResourceType type;
		size_t index;
		size_t version;
	};

	/// @brief Buffer Resource Metadata is stored for buffer resource handles in the Frame Graph.
	struct BufferResourceMetadata
	{
		size_t size;
		VkBufferUsageFlags usage;
	};

	/// @brief Texture Resource Metadata is stored for texture resource handles in the Frame Graph.
	struct TextureResourceMetadata
	{
		VkExtent2D extent;
		VkFormat format;
		VkImageUsageFlags usage;
	};

	/// @brief A TextureResource links an image and its allocation, allowing for easy creation & destruction of resources.
	struct TextureResource
	{
		VkImage image				= VK_NULL_HANDLE;
		VmaAllocation allocation	= VK_NULL_HANDLE;

		/// @brief Initialize a new Texture Resource.
		/// @param ctx RenderContext to use.
		/// @param format Image Format.
		/// @param extent Image extent. (resolution)
		/// @param usage Image usage flags.
		/// @return A new TextureResource.
		static TextureResource init(
			RenderContext* ctx,
			VkFormat format,
			VkExtent2D extent,
			VkImageUsageFlags usage
		);

		/// @brief Destroy a Texture Resource.
		/// @param ctx RenderContext used to create the resource.
		/// @param texture TextureResource to destroy.
		static void destroy(RenderContext* ctx, TextureResource& texture);
	};

	/// @brief The IFrameGraphNode interface exposes functions for rendering using a higher level interface.
	class IFrameGraphNode
	{
		friend class FrameGraph;
	public:
		/// @brief Create a new Frame Graph Node.
		/// @param name Name of the Graph Node.
		/// @param frameGraph FrameGraph to register node with.
		IFrameGraphNode(const std::string& name, FrameGraph& frameGraph);

		/// @brief Overrideable execute method, this method is used to record command buffers for a node.
		/// @param commandBuffer The command buffer to record into.
		virtual void execute(VkCommandBuffer) const = 0;

		/// @brief Overideable resource creation method. Use this method to create per node resources.
		/// @param ctx Render Context to use.
		virtual void createResources(RenderContext* ctx) = 0;

		/// @brief Overideable resource destruction method. Use this method to destroy per node resources.
		/// @param ctx Render Context to use.
		virtual void destroyResources(RenderContext* ctx) = 0;

		/// @brief Add a read dependency to this graph node.
		/// @param resource Resource handle.
		virtual void read(VirtualResourceHandle& resource);

		/// @brief Add a write dependency to this graph node.
		/// @param resource Resource handle. 
		virtual void write(VirtualResourceHandle& resource);

	protected:
		FrameGraph& m_frameGraph;
		std::string m_name = std::string("IFrameGraphNode");
		std::vector<VirtualResourceHandle> m_readDependencies	= {};
		std::vector<VirtualResourceHandle> m_writeDependencies	= {};
	};

	/// @brief Raster type graph node, executes a rasterization pipeline.
	class RasterFrameGraphNode
		:
		public IFrameGraphNode
	{
	public:
		/// @brief Create a new Raster type Frame Graph Node
		/// @param name 
		/// @param frameGraph 
		RasterFrameGraphNode(const std::string& name, FrameGraph& frameGraph) : IFrameGraphNode(name, frameGraph) {};

		/// @brief Execute this raster pass.
		/// @param commandBuffer Command buffer to record into.
		virtual void execute(VkCommandBuffer commandBuffer) const override;

		/// @brief Create node resources.
		/// @param ctx Render Context to use.
		virtual void createResources(RenderContext* ctx) override;

		/// @brief Destroy node resources.
		/// @param ctx Render Context to use.
		virtual void destroyResources(RenderContext* ctx) override;

		virtual void read(VirtualResourceHandle& resource) override;

		/// @brief Register a Render Target to this Raster Pass.
		/// @param resource Resource to be used as render target.
		/// @param loadOp Load operation.
		/// @param storeOp Store operation.
		/// @param clearValue Clear value for this attachment.
		virtual void renderTarget(
			VirtualResourceHandle& resource,
			VkAttachmentLoadOp loadOp,
			VkAttachmentStoreOp storeOp,
			VkClearValue clearValue = VkClearValue{{ 0.0f, 0.0f, 0.0f, 0.0f }}
		);

		/// @brief Register a Depth Stencil target to this Raster Pass.
		/// @param resource Resource to be used as depth stencil target.
		/// @param imageUsageAspect Image usage, either depth, stencil, or depth & stencil.
		/// @param loadOp Load operation for depth.
		/// @param storeOp Store operation for depth.
		/// @param stencilLoadOp Load operation for stencil.
		/// @param stencilStoreOp Store operation for stencil.
		/// @param clearValue Clear value for this attachment.
		virtual void depthStencil(
			VirtualResourceHandle& resource,
			VkImageAspectFlags imageUsageAspect,
			VkAttachmentLoadOp loadOp,
			VkAttachmentStoreOp storeOp,
			VkAttachmentLoadOp stencilLoadOp,
			VkAttachmentStoreOp stencilStoreOp,
			VkClearValue clearValue = VkClearValue{{ 1.0f, 0x00 }}
		);

	protected:
		struct RenderAttachment
		{
			VirtualResourceHandle resource;
			VkAttachmentDescription attachmentDescription;
			VkImageAspectFlags aspectFlags;
		};

		std::vector<RenderAttachment> m_attachments						= {};
		std::vector<VkClearValue> m_clearValues							= {};
		std::vector<VkAttachmentReference> m_colorAttachments			= {};
		std::optional<VkAttachmentReference> m_depthStencilAttachment	= {};

		VkExtent2D m_framebufferExtent			= VkExtent2D{ 0, 0 };
		std::vector<VkImageView> m_imageViews	= {};
		VkRenderPass m_renderPass				= VK_NULL_HANDLE;
		VkFramebuffer m_framebuffer				= VK_NULL_HANDLE;
	};

	/// @brief The Frame Graph generates a render pass & per frame render commands based on registered graph nodes.
	class FrameGraph
	{
		friend IFrameGraphNode;
	public:
		/// @brief Create a new Frame Graph.
		/// @param ctx Render context to use.
		/// @param shaderDB Shader Database to use for builtin render pass.
		FrameGraph(RenderContext* ctx, ShaderDatabase* shaderDB);

		/// @brief Destroy this Frame Graph.
		virtual ~FrameGraph();

		/// @brief Create a virtual texture resource.
		/// @param name Name of the resource.
		/// @param resolution Resolution of the texture.
		/// @param format Texture format.
		/// @param usage Expected usage of the texture.
		/// @return A new resource handle.
		VirtualResourceHandle createTextureResource(
			const std::string& name,
			VkExtent2D resolution,
			VkFormat format
		);

		/// @brief Retrieve Texture metadata registered for a virtual handle.
		/// @param resource Resource to retrieve metadata for.
		/// @return The Texture metadata registered.
		TextureResourceMetadata& getTextureMetadata(const VirtualResourceHandle& resource);

		/// @brief Retrieve a TextureResource registered for a virtual handle.
		/// @param resource Resource to retrieve data for.
		/// @return The TextureResource associated with the virtual handle.
		const TextureResource& getTextureResource(const VirtualResourceHandle& resource) const;

		/// @brief Mark a graph node as this graph's output node.
		/// @param name Name of the node to mark as output.
		void markOutputNode(const std::string& name);

		/// @brief Execute the Frame Graph. NOTE: generate the frame graph before execution!
		/// @param commandBuffer Command buffer to record into.
		/// @param activeSwapImageIdx Active swap image to use as final output.
		void execute(VkCommandBuffer commandBuffer, uint32_t activeSwapImageIdx) const;

		/// @brief Generate a new Frame Graph from the currently registered nodes.
		void generate();

		/// @brief Clear the Frame Graph's internal state, cleaning up all acquired resources.
		void clear();

	private:
		/// @brief Allocate a new virtual resource handle.
		/// @param name Name of the handle to allocate.
		/// @return A new resource handle.
		VirtualResourceHandle allocateResource(const std::string& name);

		/// @brief Create frame graph resources such as frame buffers and render targets.
		void createFrameGraphResources();

		/// @brief Destroy the current frame graph resources.
		void destroyFrameGraphResources();

		/// @brief Calculate the number of node parents in a work list.
		/// @param pNode The node for which to check parent count.
		/// @param workList The work list to use for verification.
		/// @return The number of parents in the work list.
		size_t parentCountInWorkList(IFrameGraphNode* pNode, const std::vector<IFrameGraphNode*>& workList) const;

		/// @brief Do a topological sort of the stored graph nodes.
		void doTopologicalSort();

	private:
		RenderContext* m_pCtx = nullptr;
		BuiltinRenderPass m_builtinRenderPass = BuiltinRenderPass();

		std::map<std::string, BufferResourceMetadata> m_bufferMetadata		= {};
		std::map<std::string, TextureResourceMetadata> m_textureMetadata	= {};
		std::vector<VirtualResourceHandle> m_resourceHandles				= {};
		std::map<std::string, TextureResource> m_textureResources			= {};

		size_t m_outputNodeIndex = 0;
		std::vector<IFrameGraphNode*> m_graphNodes					= {};
		std::vector<std::vector<IFrameGraphNode*>> m_graphTopology	= {};
	};
}
