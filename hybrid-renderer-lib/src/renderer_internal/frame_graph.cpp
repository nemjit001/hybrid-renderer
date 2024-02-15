#include "renderer_internal/frame_graph.h"

#include <cassert>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "renderer_internal/render_context.h"

using namespace hri;

TextureResource TextureResource::init(
	RenderContext* ctx,
	VkFormat format,
	VkExtent2D extent,
	VkImageUsageFlags usage
)
{
	assert(ctx != nullptr);
	TextureResource texture = TextureResource{};

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
	HRI_VK_CHECK(vmaCreateImage(ctx->allocator, &imageCreateInfo, &imageAllocationInfo, &texture.image, &texture.allocation, nullptr));

	return texture;
}

void TextureResource::destroy(RenderContext* ctx, TextureResource& texture)
{
	assert(ctx != nullptr);

	vmaDestroyImage(ctx->allocator, texture.image, texture.allocation);
	memset(&texture, 0, sizeof(TextureResource));
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
	rpBeginInfo.renderArea = VkRect2D{
		0, 0,
		m_framebufferExtent.width, m_framebufferExtent.height
	};
	rpBeginInfo.clearValueCount = static_cast<uint32_t>(m_clearValues.size());
	rpBeginInfo.pClearValues = m_clearValues.data();
	vkCmdBeginRenderPass(commandBuffer, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	// TODO: execute draw here

	vkCmdEndRenderPass(commandBuffer);
}

void RasterFrameGraphNode::createResources(RenderContext* ctx)
{
	assert(ctx != nullptr);

	m_framebufferExtent = VkExtent2D{ 0, 0 };
	std::vector<VkAttachmentDescription> attachmentDescriptions = {}; attachmentDescriptions.reserve(m_attachments.size());
	m_imageViews.reserve(m_attachments.size());
	for (auto const& attachment : m_attachments)
	{
		const TextureResourceMetadata& meta = m_frameGraph.getTextureMetadata(attachment.resource);
		const TextureResource& texture = m_frameGraph.getTextureResource(attachment.resource);

		VkImageViewCreateInfo viewCreateInfo = VkImageViewCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		viewCreateInfo.flags = 0;
		viewCreateInfo.image = texture.image;
		viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewCreateInfo.format = meta.format;
		viewCreateInfo.components = VkComponentMapping{};
		viewCreateInfo.subresourceRange = VkImageSubresourceRange{
			attachment.aspectFlags,
			0, 1, 0, 1,
		};

		VkImageView view = VK_NULL_HANDLE;
		HRI_VK_CHECK(vkCreateImageView(ctx->device, &viewCreateInfo, nullptr, &view));

		m_framebufferExtent = meta.extent;
		attachmentDescriptions.push_back(attachment.attachmentDescription);
		m_imageViews.push_back(view);
	}

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
	passCreateInfo.attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size());
	passCreateInfo.pAttachments = attachmentDescriptions.data();
	passCreateInfo.subpassCount = 1;
	passCreateInfo.pSubpasses = &subpass;
	passCreateInfo.dependencyCount = 0;
	passCreateInfo.pDependencies = nullptr;
	HRI_VK_CHECK(vkCreateRenderPass(ctx->device, &passCreateInfo, nullptr, &m_renderPass));

	VkFramebufferCreateInfo fbCreateInfo = VkFramebufferCreateInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	fbCreateInfo.flags = 0;
	fbCreateInfo.renderPass = m_renderPass;
	fbCreateInfo.attachmentCount = static_cast<uint32_t>(m_imageViews.size());
	fbCreateInfo.pAttachments = m_imageViews.data();
	fbCreateInfo.width = m_framebufferExtent.width;
	fbCreateInfo.height = m_framebufferExtent.height;
	fbCreateInfo.layers = 1;
	HRI_VK_CHECK(vkCreateFramebuffer(ctx->device, &fbCreateInfo, nullptr, &m_framebuffer));
}

void RasterFrameGraphNode::destroyResources(RenderContext* ctx)
{
	assert(ctx != nullptr);

	for (auto const& view : m_imageViews)
	{
		vkDestroyImageView(ctx->device, view, nullptr);
	}
	m_imageViews.clear();

	vkDestroyRenderPass(ctx->device, m_renderPass, nullptr);
	vkDestroyFramebuffer(ctx->device, m_framebuffer, nullptr);
}

void RasterFrameGraphNode::read(VirtualResourceHandle& resource)
{
	IFrameGraphNode::read(resource);

	// Handle all read resources as sampled image inputs for shaders.
	if (resource.type == ResourceType::Texture)
	{
		TextureResourceMetadata& meta = m_frameGraph.getTextureMetadata(resource);
		meta.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
	}
}

void RasterFrameGraphNode::renderTarget(
	VirtualResourceHandle& resource,
	VkAttachmentLoadOp loadOp,
	VkAttachmentStoreOp storeOp,
	VkClearValue clearValue
)
{
	RasterFrameGraphNode::write(resource);

	TextureResourceMetadata& meta = m_frameGraph.getTextureMetadata(resource);
	meta.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

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

	RenderAttachment renderAttachment = RenderAttachment{
		resource,
		colorAttachment,
		VK_IMAGE_ASPECT_COLOR_BIT,
	};

	VkAttachmentReference attachmentRef = VkAttachmentReference{
		static_cast<uint32_t>(m_attachments.size()),
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	};

	m_attachments.push_back(renderAttachment);
	m_clearValues.push_back(clearValue);
	m_colorAttachments.push_back(attachmentRef);
}

void RasterFrameGraphNode::depthStencil(
	VirtualResourceHandle& resource,
	VkImageAspectFlags imageUsageAspect,
	VkAttachmentLoadOp loadOp,
	VkAttachmentStoreOp storeOp,
	VkAttachmentLoadOp stencilLoadOp,
	VkAttachmentStoreOp stencilStoreOp,
	VkClearValue clearValue
)
{
	write(resource);
	
	TextureResourceMetadata& meta = m_frameGraph.getTextureMetadata(resource);
	meta.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

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

	RenderAttachment renderAttachment = RenderAttachment{
		resource,
		depthStencilAttachment,
		imageUsageAspect,
	};
	
	m_depthStencilAttachment = VkAttachmentReference{
		static_cast<uint32_t>(m_attachments.size()),
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
	};

	m_attachments.push_back(renderAttachment);
	m_clearValues.push_back(clearValue);
}

BuiltinRenderPass::BuiltinRenderPass(RenderContext* ctx, ShaderDatabase* shaderDB)
	:
	m_pCtx(ctx)
{
	assert(m_pCtx != nullptr);

	// TODO: create builtin shaders & pipeline

	VkAttachmentReference swapRef = VkAttachmentReference{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	VkAttachmentDescription swapAttachment = VkAttachmentDescription{};
	swapAttachment.flags = 0;
	swapAttachment.format = m_pCtx->swapchain.image_format;
	swapAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	swapAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	swapAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	swapAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	swapAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	swapAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	swapAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkSubpassDescription subpass = VkSubpassDescription{};
	subpass.flags = 0;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &swapRef;

	VkRenderPassCreateInfo passCreateInfo = VkRenderPassCreateInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	passCreateInfo.flags = 0;
	passCreateInfo.attachmentCount = 1;
	passCreateInfo.pAttachments = &swapAttachment;
	passCreateInfo.subpassCount = 1;
	passCreateInfo.pSubpasses = &subpass;
	passCreateInfo.dependencyCount = 0;
	passCreateInfo.pDependencies = nullptr;
	HRI_VK_CHECK(vkCreateRenderPass(m_pCtx->device, &passCreateInfo, nullptr, &m_renderPass));
	recreatePassResources();
}

BuiltinRenderPass::~BuiltinRenderPass()
{
	for (auto const& framebuffer : m_framebuffers)
	{
		vkDestroyFramebuffer(m_pCtx->device, framebuffer, nullptr);
	}

	vkDestroyRenderPass(m_pCtx->device, m_renderPass, nullptr);
	m_pCtx->swapchain.destroy_image_views(m_swapViews);
}

void BuiltinRenderPass::recreatePassResources()
{
	for (auto const& framebuffer : m_framebuffers)
	{
		vkDestroyFramebuffer(m_pCtx->device, framebuffer, nullptr);
	}
	m_pCtx->swapchain.destroy_image_views(m_swapViews);
	m_framebuffers.clear();

	VkExtent2D swapExtent = m_pCtx->swapchain.extent;
	m_swapViews = m_pCtx->swapchain.get_image_views().value();
	m_framebuffers.reserve(m_swapViews.size());
	for (auto const& view : m_swapViews)
	{
		VkFramebufferCreateInfo fbCreateInfo = VkFramebufferCreateInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
		fbCreateInfo.flags = 0;
		fbCreateInfo.renderPass = m_renderPass;
		fbCreateInfo.attachmentCount = 1;
		fbCreateInfo.pAttachments = &view;
		fbCreateInfo.width = swapExtent.width;
		fbCreateInfo.height = swapExtent.height;
		fbCreateInfo.layers = 1;

		VkFramebuffer framebuffer = VK_NULL_HANDLE;
		vkCreateFramebuffer(m_pCtx->device, &fbCreateInfo, nullptr, &framebuffer);
		m_framebuffers.push_back(framebuffer);
	}
}

void BuiltinRenderPass::execute(VkCommandBuffer commandBuffer, uint32_t activeSwapImage) const
{
	assert(activeSwapImage < m_framebuffers.size());
	VkExtent2D swapExtent = m_pCtx->swapchain.extent;
	VkFramebuffer activeFramebuffer = m_framebuffers[activeSwapImage];

	VkClearValue swapClearValue = VkClearValue{};	// Just clear swap to black
	VkRenderPassBeginInfo passBeginInfo = VkRenderPassBeginInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	passBeginInfo.renderPass = m_renderPass;
	passBeginInfo.framebuffer = activeFramebuffer;
	passBeginInfo.renderArea = VkRect2D{
		VkOffset2D{ 0, 0 },
		swapExtent
	};
	passBeginInfo.clearValueCount = 1;
	passBeginInfo.pClearValues = &swapClearValue;
	vkCmdBeginRenderPass(commandBuffer, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	// TODO: execute present shaders & pipeline w/ bound resources

	vkCmdEndRenderPass(commandBuffer);
}

GraphicsPipelineBuilder BuiltinRenderPass::builtinPipelineBuilder() const
{
	// TODO: set up present pipeline config (static variable?)
	return GraphicsPipelineBuilder{};
}

FrameGraph::FrameGraph(RenderContext* ctx, ShaderDatabase* shaderDB)
	:
	m_pCtx(ctx),
	m_builtinRenderPass(ctx, shaderDB)
{
	assert(ctx != nullptr);
}

FrameGraph::~FrameGraph()
{
	clear();
}

VirtualResourceHandle FrameGraph::createTextureResource(
	const std::string& name,
	VkExtent2D resolution,
	VkFormat format
)
{
	VirtualResourceHandle resource = allocateResource(name);

	const auto& it = m_textureMetadata.insert(
		std::make_pair(name, TextureResourceMetadata{ resolution, format, 0 })
	);

	assert(it.second != false);
	return resource;
}

TextureResourceMetadata& FrameGraph::getTextureMetadata(const VirtualResourceHandle& resource)
{
	return m_textureMetadata.at(resource.name);
}

const TextureResource& FrameGraph::getTextureResource(const VirtualResourceHandle& resource) const
{
	return m_textureResources.at(resource.name);
}

void FrameGraph::markOutputNode(const std::string& name)
{
	assert(!m_graphNodes.empty());

	m_outputNodeIndex = 0;
	for (auto const& node : m_graphNodes)
	{
		if (node->m_name == name)
			break;

		m_outputNodeIndex++;
	}

	// TODO: check if output node has 1 color attachment as output
}

void FrameGraph::execute(VkCommandBuffer commandBuffer, uint32_t activeSwapImageIdx) const
{
	for (auto const& nodeList : m_graphTopology)
	{
		for (auto const* pNode : nodeList)
		{
			pNode->execute(commandBuffer);
		}

		VkMemoryBarrier memoryBarrier = VkMemoryBarrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
		memoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		memoryBarrier.dstAccessMask = VK_ACCESS_NONE;
		vkCmdPipelineBarrier(
			commandBuffer,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			0,
			1, &memoryBarrier,
			0, nullptr,
			0, nullptr
		);
	}

	// TODO: set output node's color attachment as sampled input for default pass
	m_builtinRenderPass.execute(commandBuffer, activeSwapImageIdx);
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

	m_builtinRenderPass.recreatePassResources();
	createFrameGraphResources();
}

void FrameGraph::clear()
{
	destroyFrameGraphResources();

	m_bufferMetadata.clear();
	m_textureMetadata.clear();
	m_resourceHandles.clear();
	m_textureResources.clear();

	m_outputNodeIndex = 0;
	m_graphNodes.clear();
	m_graphTopology.clear();
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
	// Generate graph resources from virtual handles
	for (auto const& resourceHandle : m_resourceHandles)
	{
		TextureResourceMetadata meta = m_textureMetadata[resourceHandle.name];
		TextureResource texture = TextureResource::init(
			m_pCtx,
			meta.format,
			meta.extent,
			meta.usage
		);

		m_textureResources.insert(std::make_pair(resourceHandle.name, texture));
	}

	// Generate node resources
	for (auto const& pNode : m_graphNodes)
	{
		pNode->createResources(m_pCtx);
	}
}

void FrameGraph::destroyFrameGraphResources()
{
	// Clear graph resources
	for (auto& [ name, texture ] : m_textureResources)
	{
		TextureResource::destroy(m_pCtx, texture);
	}
	m_textureResources.clear();

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
