#pragma once

#include <cassert>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "renderer_internal/render_context.h"

namespace hri
{
	struct RenderTarget
	{
		VkImage image				= VK_NULL_HANDLE;
		VkImageView view			= VK_NULL_HANDLE;
		VmaAllocation allocation	= VK_NULL_HANDLE;

		static RenderTarget init(
			RenderContext* ctx,
			VkFormat format,
			VkExtent2D extent,
			VkImageUsageFlags usage,
			VkImageAspectFlags imageAspect
		)
		{
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

		static void destroy(RenderContext* ctx, RenderTarget& renderTarget)
		{
			assert(ctx);

			vmaDestroyImage(ctx->allocator, renderTarget.image, renderTarget.allocation);
			vkDestroyImageView(ctx->device, renderTarget.view, nullptr);

			memset(&renderTarget, 0, sizeof(RenderTarget));
		}
	};

	class FrameGraph
	{
	public:
		FrameGraph(RenderContext* ctx)
			:
			m_pCtx(ctx)
		{
			assert(ctx != nullptr);
			VkExtent2D swapExtent = m_pCtx->swapchain.extent;

			// Initialize render targets
			m_gbufferDepthTarget = RenderTarget::init(
				ctx,
				VK_FORMAT_D32_SFLOAT,
				swapExtent,
				VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
				VK_IMAGE_ASPECT_DEPTH_BIT
			);

			m_gbufferNormalTarget = RenderTarget::init(
				ctx,
				VK_FORMAT_R8G8B8A8_SNORM,
				swapExtent,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
				VK_IMAGE_ASPECT_COLOR_BIT
			);

			m_gbufferAlbedoTarget = RenderTarget::init(
				ctx,
				VK_FORMAT_R8G8B8A8_UNORM,
				swapExtent,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
				VK_IMAGE_ASPECT_COLOR_BIT
			);

			// Set up deferred rendering subpass
			VkAttachmentReference gbufferDepthRef = VkAttachmentReference{ 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
			VkAttachmentDescription gbufferDepth = VkAttachmentDescription{};
			gbufferDepth.flags = 0;
			gbufferDepth.format = VK_FORMAT_D32_SFLOAT;
			gbufferDepth.samples = VK_SAMPLE_COUNT_1_BIT;
			gbufferDepth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			gbufferDepth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			gbufferDepth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			gbufferDepth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
			gbufferDepth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			gbufferDepth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			VkAttachmentReference gbufferNormalRef = VkAttachmentReference{ 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
			VkAttachmentDescription gbufferNormal = VkAttachmentDescription{};
			gbufferNormal.flags = 0;
			gbufferNormal.format = VK_FORMAT_R8G8B8A8_SNORM;
			gbufferNormal.samples = VK_SAMPLE_COUNT_1_BIT;
			gbufferNormal.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			gbufferNormal.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			gbufferNormal.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			gbufferNormal.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			gbufferNormal.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			gbufferNormal.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			VkAttachmentReference gbufferAlbedoRef = VkAttachmentReference{ 3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
			VkAttachmentDescription gbufferAlbedo = VkAttachmentDescription{};
			gbufferAlbedo.flags = 0;
			gbufferAlbedo.format = VK_FORMAT_R8G8B8A8_UNORM;
			gbufferAlbedo.samples = VK_SAMPLE_COUNT_1_BIT;
			gbufferAlbedo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			gbufferAlbedo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			gbufferAlbedo.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			gbufferAlbedo.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			gbufferAlbedo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			gbufferAlbedo.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			VkAttachmentReference gbufferColorAttachments[] = {
				gbufferNormalRef,
				gbufferAlbedoRef,
			};

			VkSubpassDescription gBufferSubpass = VkSubpassDescription{};
			gBufferSubpass.flags = 0;
			gBufferSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			gBufferSubpass.inputAttachmentCount = 0;
			gBufferSubpass.pInputAttachments = nullptr;
			gBufferSubpass.colorAttachmentCount = HRI_SIZEOF_ARRAY(gbufferColorAttachments);
			gBufferSubpass.pColorAttachments = gbufferColorAttachments;
			gBufferSubpass.pResolveAttachments = nullptr;
			gBufferSubpass.pDepthStencilAttachment = &gbufferDepthRef;
			gBufferSubpass.preserveAttachmentCount = 0;
			gBufferSubpass.pPreserveAttachments = nullptr;

			// Set up presentation subpass
			VkAttachmentReference swapAttachmentColorRef = VkAttachmentReference{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
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

			VkSubpassDescription presentSubpass = VkSubpassDescription{};
			presentSubpass.flags = 0;
			presentSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			presentSubpass.inputAttachmentCount = 0;
			presentSubpass.pInputAttachments = nullptr;
			presentSubpass.colorAttachmentCount = 1;
			presentSubpass.pColorAttachments = &swapAttachmentColorRef;
			presentSubpass.pResolveAttachments = nullptr;
			presentSubpass.pDepthStencilAttachment = nullptr;
			presentSubpass.preserveAttachmentCount = 0;
			presentSubpass.pPreserveAttachments = nullptr;

			// Set up subpass dependencies
			VkSubpassDependency presentDependency = VkSubpassDependency{};
			presentDependency.srcSubpass = 0;	// gbuffer pass
			presentDependency.dstSubpass = 1;	// present pass
			presentDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			presentDependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			presentDependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			presentDependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			presentDependency.dependencyFlags = 0;

			// Set up render pass
			VkAttachmentDescription attachments[] = {
				swapAttachment,
				gbufferDepth,
				gbufferNormal,
				gbufferAlbedo,
			};

			VkSubpassDescription subpasses[] = {
				gBufferSubpass,
				presentSubpass,
			};

			VkSubpassDependency dependencies[] = {
				presentDependency,
			};

			VkRenderPassCreateInfo renderPassCreateInfo = VkRenderPassCreateInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
			renderPassCreateInfo.flags = 0;
			renderPassCreateInfo.attachmentCount = HRI_SIZEOF_ARRAY(attachments);
			renderPassCreateInfo.pAttachments = attachments;
			renderPassCreateInfo.subpassCount = HRI_SIZEOF_ARRAY(subpasses);
			renderPassCreateInfo.pSubpasses = subpasses;
			renderPassCreateInfo.dependencyCount = HRI_SIZEOF_ARRAY(dependencies);
			renderPassCreateInfo.pDependencies = dependencies;
			HRI_VK_CHECK(vkCreateRenderPass(m_pCtx->device, &renderPassCreateInfo, nullptr, &m_renderPass));

			// Create framebuffers for render pass
			m_swapViews = m_pCtx->swapchain.get_image_views().value();
			for (auto const& swapView : m_swapViews)
			{
				VkImageView fbAttachments[] = {
					swapView,
					m_gbufferDepthTarget.view,
					m_gbufferNormalTarget.view,
					m_gbufferAlbedoTarget.view,
				};

				VkFramebufferCreateInfo fbCreateInfo = VkFramebufferCreateInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
				fbCreateInfo.flags = 0;
				fbCreateInfo.attachmentCount = HRI_SIZEOF_ARRAY(fbAttachments);
				fbCreateInfo.pAttachments = fbAttachments;
				fbCreateInfo.renderPass = m_renderPass;
				fbCreateInfo.width	= swapExtent.width;
				fbCreateInfo.height = swapExtent.height;
				fbCreateInfo.layers = 1;

				VkFramebuffer framebuffer = VK_NULL_HANDLE;
				HRI_VK_CHECK(vkCreateFramebuffer(m_pCtx->device, &fbCreateInfo, nullptr, &framebuffer));
				m_framebuffers.push_back(framebuffer);
			}
		}

		virtual ~FrameGraph()
		{
			RenderTarget::destroy(m_pCtx, m_gbufferDepthTarget);
			RenderTarget::destroy(m_pCtx, m_gbufferNormalTarget);
			RenderTarget::destroy(m_pCtx, m_gbufferAlbedoTarget);

			vkDestroyRenderPass(m_pCtx->device, m_renderPass, nullptr);
			m_pCtx->swapchain.destroy_image_views(m_swapViews);

			for (auto const& framebuffer : m_framebuffers)
			{
				vkDestroyFramebuffer(m_pCtx->device, framebuffer, nullptr);
			}
		}

	private:
		RenderContext* m_pCtx				= nullptr;
		RenderTarget m_gbufferDepthTarget	= RenderTarget{};
		RenderTarget m_gbufferNormalTarget	= RenderTarget{};
		RenderTarget m_gbufferAlbedoTarget	= RenderTarget{};
		VkRenderPass m_renderPass			= VK_NULL_HANDLE;
		std::vector<VkImageView> m_swapViews		= {};
		std::vector<VkFramebuffer> m_framebuffers	= {};
	};
}
