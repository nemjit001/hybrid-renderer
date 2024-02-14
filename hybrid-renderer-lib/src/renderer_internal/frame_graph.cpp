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
	VkRenderPassBeginInfo rpBeginInfo = VkRenderPassBeginInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	rpBeginInfo.renderPass = m_renderPass;
	rpBeginInfo.framebuffer = m_framebuffer;
	rpBeginInfo.renderArea = VkRect2D{};
	rpBeginInfo.clearValueCount = 0;
	rpBeginInfo.pClearValues = nullptr;
	vkCmdBeginRenderPass(commandBuffer, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	// TODO: execute draw here

	vkCmdEndRenderPass(commandBuffer);
}

void RasterFrameGraphNode::createResources(RenderContext* ctx)
{
	assert(ctx != nullptr);

	VkSubpassDescription subpass = VkSubpassDescription{};
	subpass.flags = 0;
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.inputAttachmentCount = 0;
	subpass.pInputAttachments = nullptr;
	subpass.colorAttachmentCount = static_cast<uint32_t>(m_colorAttachments.size());
	subpass.pColorAttachments = m_colorAttachments.data();
	subpass.pResolveAttachments = nullptr;
	subpass.pDepthStencilAttachment = m_depthStencilAttachment.has_value() ? &m_depthStencilAttachment.value() : (nullptr);
	subpass.preserveAttachmentCount = 0;
	subpass.pPreserveAttachments = nullptr;

	VkRenderPassCreateInfo passCreateInfo = VkRenderPassCreateInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	passCreateInfo.flags = 0;
	passCreateInfo.attachmentCount = static_cast<uint32_t>(m_attachments.size());
	passCreateInfo.pAttachments = m_attachments.data();
	passCreateInfo.subpassCount = 1;
	passCreateInfo.pSubpasses = &subpass;
	passCreateInfo.dependencyCount = 0;
	passCreateInfo.pDependencies = nullptr;
	HRI_VK_CHECK(vkCreateRenderPass(ctx->device, &passCreateInfo, nullptr, &m_renderPass));

	// TODO: load resource attachments from Frame Graph
	VkFramebufferCreateInfo fbCreateInfo = VkFramebufferCreateInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	fbCreateInfo.flags = 0;
	fbCreateInfo.renderPass = m_renderPass;
	fbCreateInfo.attachmentCount = 0;
	fbCreateInfo.pAttachments = nullptr;
	fbCreateInfo.width = 0;
	fbCreateInfo.height = 0;
	fbCreateInfo.layers = 1;
	HRI_VK_CHECK(vkCreateFramebuffer(ctx->device, &fbCreateInfo, nullptr, &m_framebuffer));
}

void RasterFrameGraphNode::destroyResources(RenderContext* ctx)
{
	assert(ctx != nullptr);
	vkDestroyRenderPass(ctx->device, m_renderPass, nullptr);
	vkDestroyFramebuffer(ctx->device, m_framebuffer, nullptr);
}

void RasterFrameGraphNode::renderTarget(VirtualResourceHandle& resource, VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp)
{
	write(resource);

	const TextureResourceMetadata& meta = m_frameGraph.getTextureMetadata(resource.name);
	VkAttachmentDescription colorAttachment = VkAttachmentDescription{};
	colorAttachment.flags = 0;
	colorAttachment.format = meta.format;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = loadOp;
	colorAttachment.storeOp = storeOp;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	m_colorAttachments.push_back(VkAttachmentReference{
		static_cast<uint32_t>(m_attachments.size()),
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	});

	m_attachments.push_back(colorAttachment);
}

void RasterFrameGraphNode::depthStencil(
	VirtualResourceHandle& resource,
	VkAttachmentLoadOp loadOp,
	VkAttachmentStoreOp storeOp,
	VkAttachmentLoadOp stencilLoadOp,
	VkAttachmentStoreOp stencilStoreOp
)
{
	write(resource);
	
	const TextureResourceMetadata& meta = m_frameGraph.getTextureMetadata(resource.name);
	VkAttachmentDescription depthStencilAttachment = VkAttachmentDescription{};
	depthStencilAttachment.flags = 0;
	depthStencilAttachment.format = meta.format;
	depthStencilAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthStencilAttachment.loadOp = loadOp;
	depthStencilAttachment.storeOp = storeOp;
	depthStencilAttachment.stencilLoadOp = stencilLoadOp;
	depthStencilAttachment.stencilStoreOp = stencilStoreOp;
	depthStencilAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthStencilAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	m_depthStencilAttachment = VkAttachmentReference{
		static_cast<uint32_t>(m_attachments.size()),
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
	};

	m_attachments.push_back(depthStencilAttachment);
}

void PresentFrameGraphNode::execute(VkCommandBuffer commandBuffer) const
{
	// TODO: implement
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

	const auto& it = m_bufferMetadata.insert(std::make_pair(name, BufferResourceMetadata{ size, usage }));
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

	const auto& it = m_textureMetadata.insert(std::make_pair(name, TextureResourceMetadata{ resolution, format, usage }));
	assert(it.second != false);

	return resource;
}

const TextureResourceMetadata& FrameGraph::getTextureMetadata(const std::string& name) const
{
	return m_textureMetadata.at(name);
}

void FrameGraph::markOutputNode(const std::string& name)
{
	m_outputNodeIndex = 0;
	for (auto const& node : m_graphNodes)
	{
		if (node->m_name == name)
			break;

		m_outputNodeIndex++;
	}
}

void FrameGraph::execute(VkCommandBuffer commandBuffer, uint32_t activeSwapImageIdx) const
{
	for (auto const& nodeList : m_graphTopology)
	{
		for (auto const* pNode : nodeList)
		{
			pNode->execute(commandBuffer);
		}

		// TODO: add memory barrier here
	}
}

void FrameGraph::generate()
{
	assert(!m_graphNodes.empty());

	destroyFrameGraphResources();
	m_graphTopology.clear();

	size_t idx = 0;
	for (auto const& pNode : m_graphNodes)
	{
		printf("`%s` node\n", pNode->m_name.c_str());
		printf("\tRead Dependencies:\n");
		for (auto const& dep : pNode->m_readDependencies)
			printf("\t - index: %zu version: %zu (%s)\n", dep.index, dep.version, dep.name.c_str());

		printf("\tWrite Dependencies:\n");
		for (auto const& dep : pNode->m_writeDependencies)
			printf("\t - index: %zu version: %zu (%s)\n", dep.index, dep.version, dep.name.c_str());

		idx++;
	}

	// TODO: dead strip nodes
	doTopologicalSort();

	for (auto const& pass : m_graphTopology)
	{
		printf("[\n");
		for (auto const& pNode : pass)
			printf("\t%s\n", pNode->m_name.c_str());
		printf("]\n");
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
	// Generate node resources
	for (auto const& pNode : m_graphNodes)
	{
		pNode->createResources(m_pCtx);
	}
}

void FrameGraph::destroyFrameGraphResources()
{
	// Destroy node resources
	for (auto const& pNode : m_graphNodes)
	{
		pNode->destroyResources(m_pCtx);
	}
}

size_t FrameGraph::parentCountInWorkList(IFrameGraphNode* pNode, const std::vector<IFrameGraphNode*>& workList) const
{
	size_t parentCount = 0;

	// XXX: this might be ugly & slow, check if this bottlenecks performance in large frame graph.
	for (auto const& readDep : pNode->m_readDependencies)
	{
		if (readDep.version == 0)
			continue;

		size_t targetVersion = readDep.version - 1;
		for (auto const& pOther : workList)
		{
			for (auto const& writeDep : pOther->m_writeDependencies)
			{
				if (writeDep.index == readDep.index && writeDep.version == targetVersion)
				{
					parentCount++;
				}
			}
		}
	}

	return parentCount;
}

void FrameGraph::doTopologicalSort()
{
	m_graphTopology.clear();
	std::vector<IFrameGraphNode*> workList = m_graphNodes;
	std::vector<IFrameGraphNode*> leftOverList = {};
	
	while (!workList.empty())
	{
		m_graphTopology.push_back({});

		for (auto const& pNode : workList)
		{
			if (parentCountInWorkList(pNode, workList) == 0)
			{
				m_graphTopology.back().push_back(pNode);
			}
			else
			{
				leftOverList.push_back(pNode);
			}
		}

		// If the current graph layer is empty, we encountered a cyclic graph
		assert(!m_graphTopology.back().empty());

		workList = leftOverList;
		leftOverList.clear();
	}
}
