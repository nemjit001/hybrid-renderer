#include "renderer_internal/frame_graph.h"

#include <cassert>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "renderer_internal/render_context.h"

using namespace hri;

RenderTarget RenderTarget::init(
	RenderContext* ctx,
	VkFormat format,
	VkExtent2D extent,
	VkImageUsageFlags usage,
	VkImageAspectFlags imageAspect
)
{
	assert(ctx != nullptr);
	RenderTarget target = RenderTarget{};

	VkImageCreateInfo imageCreateInfo = VkImageCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	imageCreateInfo.flags = 0;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = format;
	imageCreateInfo.extent = VkExtent3D{ extent.width, extent.height, 1 };
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.usage = usage;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.queueFamilyIndexCount = 0;
	imageCreateInfo.pQueueFamilyIndices = nullptr;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VmaAllocationCreateInfo imageAllocationInfo = VmaAllocationCreateInfo{};
	imageAllocationInfo.flags = 0;
	imageAllocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
	HRI_VK_CHECK(vmaCreateImage(ctx->allocator, &imageCreateInfo, &imageAllocationInfo, &target.image, &target.allocation, nullptr));

	VkImageViewCreateInfo viewCreateInfo = VkImageViewCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	viewCreateInfo.flags = 0;
	viewCreateInfo.image = target.image;
	viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewCreateInfo.format = format;
	viewCreateInfo.components = VkComponentMapping{
		VK_COMPONENT_SWIZZLE_IDENTITY,
		VK_COMPONENT_SWIZZLE_IDENTITY,
		VK_COMPONENT_SWIZZLE_IDENTITY,
		VK_COMPONENT_SWIZZLE_IDENTITY,
	};
	viewCreateInfo.subresourceRange = VkImageSubresourceRange{
		imageAspect,
		0, 1,
		0, 1,
	};
	HRI_VK_CHECK(vkCreateImageView(ctx->device, &viewCreateInfo, nullptr, &target.view));

	return target;
}

void RenderTarget::destroy(RenderContext* ctx, RenderTarget& renderTarget)
{
	assert(ctx != nullptr);

	vmaDestroyImage(ctx->allocator, renderTarget.image, renderTarget.allocation);
	vkDestroyImageView(ctx->device, renderTarget.view, nullptr);

	memset(&renderTarget, 0, sizeof(RenderTarget));
}

IFrameGraphNode::IFrameGraphNode(const std::string& name, FrameGraph& frameGraph)
	:
	m_frameGraph(frameGraph),
	m_name(name)
{
	m_frameGraph.m_graphNodes.push_back(this);
}

void IFrameGraphNode::read(VirtualResourceHandle& resource)
{
	m_readDependencies.push_back(resource);
}

void IFrameGraphNode::write(VirtualResourceHandle& resource)
{
	m_readDependencies.push_back(resource);
	m_writeDependencies.push_back(resource);

	VirtualResourceHandle& handle = m_frameGraph.m_resourceHandles[resource.index];
	handle.version++;
	resource = handle;
}

void RasterFrameGraphNode::execute(VkCommandBuffer commandBuffer) const
{
	//
}

void PresentFrameGraphNode::execute(VkCommandBuffer commandBuffer) const
{
	//
}

FrameGraph::FrameGraph(RenderContext* ctx)
	:
	m_pCtx(ctx)
{
	assert(ctx != nullptr);
}

FrameGraph::~FrameGraph()
{
	destroyFrameGraphResources();
}

VirtualResourceHandle FrameGraph::createBufferResource(
	const std::string& name,
	size_t size,
	VkBufferUsageFlags usage
)
{
	VirtualResourceHandle resource = allocateResource(name);

	const auto& it = m_bufferMetadata.insert(std::make_pair(name, BufferMetadata{ size, usage }));
	assert(it.second != false);

	return resource;
}

VirtualResourceHandle FrameGraph::createTextureResource(
	const std::string& name,
	VkExtent2D resolution,
	VkFormat format,
	VkImageUsageFlags usage
)
{
	VirtualResourceHandle resource = allocateResource(name);

	const auto& it = m_textureMetadata.insert(std::make_pair(name, TextureMetadata{ resolution, format, usage }));
	assert(it.second != false);

	return resource;
}

void FrameGraph::execute(VkCommandBuffer commandBuffer, uint32_t activeSwapImageIdx) const
{
	// TODO: execute frame graph
}

void FrameGraph::generate()
{
	destroyFrameGraphResources();

	// TODO: create topological sort of graph nodes for execution
	for (auto const& pNode : m_graphNodes)
	{
		printf("`%s` node\n", pNode->m_name.c_str());
		printf("\tRead Dependencies:\n");
		for (auto const& dep : pNode->m_readDependencies)
			printf("\t - index: %zu version: %zu (%s)\n", dep.index, dep.version, dep.name.c_str());

		printf("\tWrite Dependencies:\n");
		for (auto const& dep : pNode->m_writeDependencies)
			printf("\t - index: %zu version: %zu (%s)\n", dep.index, dep.version, dep.name.c_str());
	}

	createFrameGraphResources();
}

VirtualResourceHandle FrameGraph::allocateResource(const std::string& name)
{
	VirtualResourceHandle resource = VirtualResourceHandle{
		name,
		ResourceType::Texture,
		m_resourceHandles.size(),
		0
	};

	m_resourceHandles.push_back(resource);
	return resource;
}

void FrameGraph::createFrameGraphResources()
{
	//
}

void FrameGraph::destroyFrameGraphResources()
{
	//
}
