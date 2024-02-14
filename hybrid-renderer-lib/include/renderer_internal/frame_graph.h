#pragma once

#include <cassert>
#include <map>
#include <optional>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "renderer_internal/render_context.h"
#include "renderer_internal/shader_database.h"

namespace hri
{
	/// @brief Forward declaration for FrameGraph class
	class FrameGraph;

	/// @brief Simple viewport representation
	struct Viewport
	{
		float x			= 0.0f;
		float y			= 0.0f;
		float width		= 0.0f;
		float height	= 0.0f;
		float minDepth	= hri::DefaultViewportMinDepth;
		float maxDepth	= hri::DefaultViewportMaxDepth;
	};

	/// @brief Simple scissor rectangle representation
	struct Scissor
	{
		struct {
			int32_t x;
			int32_t y;
		};	// Offset

		struct {
			uint32_t width;
			uint32_t height;
		};	// Extent
	};

	/// @brief Resource Type indicates a Virtual Resource's type.
	enum class ResourceType
	{
		Texture,
		Buffer,
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

	/// @brief A Render Target is a collection of an image, its view, and an allocation.
	///		Render Targets are used by the Frame Graph to store intermediary render results.
	struct RenderTarget
	{
		VkImage image				= VK_NULL_HANDLE;
		VkImageView view			= VK_NULL_HANDLE;
		VmaAllocation allocation	= VK_NULL_HANDLE;

		/// @brief Initialize a new Render Target.
		/// @param ctx Render Context to use.
		/// @param format Target image format.
		/// @param extent Target extent (resolution)
		/// @param usage Target expected usage.
		/// @param imageAspect Target image aspect.
		/// @return A newly initialized RenderTarget.
		static RenderTarget init(
			RenderContext* ctx,
			VkFormat format,
			VkExtent2D extent,
			VkImageUsageFlags usage,
			VkImageAspectFlags imageAspect
		);

		/// @brief Destroy a Render Target.
		/// @param ctx Render Context to use.
		/// @param renderTarget Render Target to destroy.
		static void destroy(RenderContext* ctx, RenderTarget& renderTarget);
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

		virtual void renderTarget(VirtualResourceHandle& resource, VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp);

		virtual void depthStencil(
			VirtualResourceHandle& resource,
			VkAttachmentLoadOp loadOp,
			VkAttachmentStoreOp storeOp,
			VkAttachmentLoadOp stencilLoadOp,
			VkAttachmentStoreOp stencilStoreOp
		);

	protected:
		std::vector<VkAttachmentDescription> m_attachments				= {};
		std::vector<VkAttachmentReference> m_colorAttachments			= {};
		std::optional<VkAttachmentReference> m_depthStencilAttachment	= {};
		VkRenderPass m_renderPass	= VK_NULL_HANDLE;
		VkFramebuffer m_framebuffer = VK_NULL_HANDLE;
	};

	/// @brief Present type raster graph node, executes a rasterization pipeline that writes into an active swap image.
	class PresentFrameGraphNode
		:
		public RasterFrameGraphNode
	{
	public:
		/// @brief Create a new Present type Frame Graph Node
		/// @param name 
		/// @param frameGraph 
		PresentFrameGraphNode(const std::string& name, FrameGraph& frameGraph) : RasterFrameGraphNode(name, frameGraph) {};

		/// @brief Execute this present pass.
		/// @param commandBuffer Command buffer to record into.
		virtual void execute(VkCommandBuffer commandBuffer) const override;
	};

	/// @brief The Frame Graph generates a render pass & per frame render commands based on registered graph nodes.
	class FrameGraph
	{
		friend IFrameGraphNode;
	public:
		/// @brief Create a new Frame Graph.
		/// @param ctx Render context to use.
		FrameGraph(RenderContext* ctx);

		/// @brief Destroy this Frame Graph.
		virtual ~FrameGraph();

		/// @brief Create a virtual buffer resource.
		/// @param name Name of the resource.
		/// @param size Size of the buffer to be created.
		/// @param usage Expected usage of the buffer.
		/// @return A new resource handle.
		VirtualResourceHandle createBufferResource(
			const std::string& name,
			size_t size,
			VkBufferUsageFlags usage
		);

		/// @brief Create a virtual texture resource.
		/// @param name Name of the resource.
		/// @param resolution Resolution of the texture.
		/// @param format Texture format.
		/// @param usage Expected usage of the texture.
		/// @return A new resource handle.
		VirtualResourceHandle createTextureResource(
			const std::string& name,
			VkExtent2D resolution,
			VkFormat format,
			VkImageUsageFlags usage
		);

		/// @brief Mark a graph node as this graph's output node.
		/// @param name Name of the node to mark as output.
		void markOutputNode(const std::string& name);

		/// @brief Execute the Frame Graph. NOTE: generate the frame graph before execution!
		/// @param commandBuffer Command buffer to record into.
		/// @param activeSwapImageIdx Active swap image to use as final output.
		void execute(VkCommandBuffer commandBuffer, uint32_t activeSwapImageIdx) const;

		/// @brief Generate a new Frame Graph from the currently registered nodes.
		void generate();

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
		/// @brief Buffer Metadata is used to create buffer handles for the Frame Graph.
		struct BufferMetadata
		{
			size_t size;
			VkBufferUsageFlags usage;
		};

		/// @brief Texture Metadata is used to create texture handles for the Frame Graph.
		struct TextureMetadata
		{
			VkExtent2D extent;
			VkFormat format;
			VkImageUsageFlags usage;
		};

		RenderContext* m_pCtx = nullptr;

		std::map<std::string, BufferMetadata> m_bufferMetadata		= {};
		std::map<std::string, TextureMetadata> m_textureMetadata	= {};
		std::vector<VirtualResourceHandle> m_resourceHandles		= {};

		size_t m_outputNodeIndex = 0;
		std::vector<IFrameGraphNode*> m_graphNodes					= {};
		std::vector<std::vector<IFrameGraphNode*>> m_graphTopology	= {};
	};
}
