#pragma once

#include <hybrid_renderer.h>
#include <vector>

class PresentPass
    :
    public hri::IRecordablePass
{
public:
    PresentPass(hri::RenderContext* ctx, hri::ShaderDatabase* shaderDB)
        :
        hri::IRecordablePass(ctx)
    {
        hri::RenderPassBuilder passBuilder = hri::RenderPassBuilder(m_pCtx);
        m_renderPass = passBuilder
            .addAttachment(
                m_pCtx->swapchain.image_format,
                VK_SAMPLE_COUNT_1_BIT,
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                VK_ATTACHMENT_LOAD_OP_CLEAR,
                VK_ATTACHMENT_STORE_OP_STORE
            )
            .setAttachmentReference(hri::AttachmentType::Color, VkAttachmentReference{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL })
            .build();

        shaderDB->registerShader("PresentVert", hri::Shader::loadFile(m_pCtx, "shaders/present.vert.spv", VK_SHADER_STAGE_VERTEX_BIT));
        shaderDB->registerShader("PresentFrag", hri::Shader::loadFile(m_pCtx, "shaders/present.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT));
        shaderDB->createPipeline("PresentPipeline", { "PresentVert", "PresentFrag" }, {}, {}); // TODO: set up pipeline layout description & pipeline builder
        m_pPresentPSO = shaderDB->getPipeline("PresentPipeline");

        createFrameResources();
    }

    virtual ~PresentPass()
    {
        vkDestroyRenderPass(m_pCtx->device, m_renderPass, nullptr);
        destroyFrameResources();
    }

    void createFrameResources()
    {
        VkExtent2D swapExtent = m_pCtx->swapchain.extent;
        m_swapViews = m_pCtx->swapchain.get_image_views().value();
        for (auto const& view : m_swapViews)
        {
            const VkImageView attachments[] = { view };

            VkFramebufferCreateInfo fbCreateInfo = VkFramebufferCreateInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
            fbCreateInfo.flags = 0;
            fbCreateInfo.renderPass = m_renderPass;
            fbCreateInfo.attachmentCount = HRI_SIZEOF_ARRAY(attachments);
            fbCreateInfo.pAttachments = attachments;
            fbCreateInfo.width = swapExtent.width;
            fbCreateInfo.height = swapExtent.height;
            fbCreateInfo.layers = 1;

            VkFramebuffer framebuffer = VK_NULL_HANDLE;
            HRI_VK_CHECK(vkCreateFramebuffer(m_pCtx->device, &fbCreateInfo, nullptr, &framebuffer));
            m_framebuffers.push_back(framebuffer);
        }
    }

    void destroyFrameResources()
    {
        m_pCtx->swapchain.destroy_image_views(m_swapViews);
        for (auto const& framebuffer : m_framebuffers)
        {
            vkDestroyFramebuffer(m_pCtx->device, framebuffer, nullptr);
        }
        m_framebuffers.clear();
    }

    inline void recreateFrameResources()
    {
        destroyFrameResources();
        createFrameResources();
    }

    virtual void record(const hri::ActiveFrame& frame) const
    {
        assert(frame.activeSwapImageIndex < m_framebuffers.size());
        assert(m_pPresentPSO != nullptr);

        VkExtent2D swapExtent = m_pCtx->swapchain.extent;
        VkClearValue clearValues[] = {
            VkClearValue{{ 0.0f, 0.0f, 0.0f, 1.0f }},   // Swap image
        };

        VkRenderPassBeginInfo passBeginInfo = VkRenderPassBeginInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
        passBeginInfo.renderPass = m_renderPass;
        passBeginInfo.framebuffer = m_framebuffers[frame.activeSwapImageIndex];
        passBeginInfo.renderArea = VkRect2D{ VkOffset2D{ 0, 0 }, swapExtent };
        passBeginInfo.clearValueCount = HRI_SIZEOF_ARRAY(clearValues);
        passBeginInfo.pClearValues = clearValues;
        vkCmdBeginRenderPass(frame.commandBuffer, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindDescriptorSets(
            frame.commandBuffer,
            m_pPresentPSO->bindPoint,
            m_pPresentPSO->layout,
            0, 0, nullptr,
            0, nullptr
        );
        vkCmdBindPipeline(frame.commandBuffer, m_pPresentPSO->bindPoint, m_pPresentPSO->pipeline);
        vkCmdDraw(frame.commandBuffer, 3, 1, 0, 0);

        vkCmdEndRenderPass(frame.commandBuffer);
    }

private:
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    std::vector<VkImageView> m_swapViews = {};
    std::vector<VkFramebuffer> m_framebuffers = {};
    const hri::PipelineStateObject* m_pPresentPSO = nullptr;
};
