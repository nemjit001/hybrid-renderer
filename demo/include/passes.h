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
        // Set up render pass
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

        // Set up color blend state
        std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments = {
            VkPipelineColorBlendAttachmentState{
                VK_FALSE,
                VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
                VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
                VK_COLOR_COMPONENT_R_BIT
                | VK_COLOR_COMPONENT_G_BIT
                | VK_COLOR_COMPONENT_B_BIT
                | VK_COLOR_COMPONENT_A_BIT
            },
        };

        // Set up pipeline descriptor layout
        hri::PipelineLayoutDescription presentPipelineLayout = hri::PipelineLayoutDescription{};
        presentPipelineLayout.descriptorSetLayouts = {
            hri::DescriptorSetLayoutDescription{
                0,
                {
                    VkDescriptorSetLayoutBinding{
                        0,
                        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        1,
                        VK_SHADER_STAGE_FRAGMENT_BIT,
                        nullptr
                    }
                }
            }
        };

        // Configure graphics pipeline
        hri::GraphicsPipelineBuilder presentPipelineBuilder = hri::GraphicsPipelineBuilder{};
        presentPipelineBuilder.vertexInputBindings = {};
        presentPipelineBuilder.vertexInputAttributes = {};
        presentPipelineBuilder.inputAssemblyState = hri::GraphicsPipelineBuilder::initInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, false);
        presentPipelineBuilder.viewport = hri::GraphicsPipelineBuilder::initDefaultViewport(1.0f, 1.0f);
        presentPipelineBuilder.scissor = hri::GraphicsPipelineBuilder::initDefaultScissor(1, 1);
        presentPipelineBuilder.rasterizationState = hri::GraphicsPipelineBuilder::initRasterizationState(false, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
        presentPipelineBuilder.multisampleState = hri::GraphicsPipelineBuilder::initMultisampleState(VK_SAMPLE_COUNT_1_BIT);
        presentPipelineBuilder.depthStencilState = hri::GraphicsPipelineBuilder::initDepthStencilState(false, false);
        presentPipelineBuilder.colorBlendState = hri::GraphicsPipelineBuilder::initColorBlendState(colorBlendAttachments);
        presentPipelineBuilder.dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        presentPipelineBuilder.renderPass = m_renderPass;
        presentPipelineBuilder.subpass = 0;

        // Register shaders & create PSO
        shaderDB->registerShader("PresentVert", hri::Shader::loadFile(m_pCtx, "shaders/present.vert.spv", VK_SHADER_STAGE_VERTEX_BIT));
        shaderDB->registerShader("PresentFrag", hri::Shader::loadFile(m_pCtx, "shaders/present.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT));
        shaderDB->createPipeline("PresentPipeline", { "PresentVert", "PresentFrag" }, presentPipelineLayout, presentPipelineBuilder);
        m_pPresentPSO = shaderDB->getPipeline("PresentPipeline");

        createFrameResources();
    }

    virtual ~PresentPass()
    {
        vkDestroyRenderPass(m_pCtx->device, m_renderPass, nullptr);
        destroyFrameResources();
    }

    virtual void createFrameResources() override
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

    virtual void destroyFrameResources() override
    {
        m_pCtx->swapchain.destroy_image_views(m_swapViews);
        for (auto const& framebuffer : m_framebuffers)
        {
            vkDestroyFramebuffer(m_pCtx->device, framebuffer, nullptr);
        }
        m_framebuffers.clear();
    }

    virtual void record(const hri::ActiveFrame& frame) const override
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

        VkViewport viewport = VkViewport{
            0.0f, 0.0f,
            static_cast<float>(swapExtent.width), static_cast<float>(swapExtent.height),
            hri::DefaultViewportMinDepth, hri::DefaultViewportMaxDepth
        };

        VkRect2D scissor = VkRect2D{
            VkOffset2D{ 0, 0 },
            swapExtent
        };

        vkCmdSetViewport(frame.commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(frame.commandBuffer, 0, 1, &scissor);

        // TODO: allocate(?) & bind descriptor sets for PSO
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
