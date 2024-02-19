#pragma once

#include <hybrid_renderer.h>
#include <vector>

class GBufferLayoutPass
    :
    public hri::IRecordablePass
{
public:
    GBufferLayoutPass(hri::RenderContext* ctx, hri::ShaderDatabase* shaderDB)
        :
        hri::IRecordablePass(ctx)
    {
        // Set up GBuffer Layout pass
        hri::RenderPassBuilder passBuilder = hri::RenderPassBuilder(m_pCtx);
        m_renderPass = passBuilder
            .addAttachment( // Albedo target
                VK_FORMAT_R8G8B8A8_UNORM,
                VK_SAMPLE_COUNT_1_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_ATTACHMENT_LOAD_OP_CLEAR,
                VK_ATTACHMENT_STORE_OP_STORE
            )
            .addAttachment( // World Pos target
                VK_FORMAT_R16G16B16_SFLOAT,
                VK_SAMPLE_COUNT_1_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_ATTACHMENT_LOAD_OP_CLEAR,
                VK_ATTACHMENT_STORE_OP_STORE
            )
            .addAttachment( // Normal target
                VK_FORMAT_R16G16B16_SFLOAT,
                VK_SAMPLE_COUNT_1_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_ATTACHMENT_LOAD_OP_CLEAR,
                VK_ATTACHMENT_STORE_OP_STORE
            )
            .addAttachment( // Depth attachment
                VK_FORMAT_D32_SFLOAT,
                VK_SAMPLE_COUNT_1_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_ATTACHMENT_LOAD_OP_CLEAR,
                VK_ATTACHMENT_STORE_OP_STORE
            )
            .setAttachmentReference(hri::AttachmentType::Color, VkAttachmentReference{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL })
            .setAttachmentReference(hri::AttachmentType::Color, VkAttachmentReference{ 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL })
            .setAttachmentReference(hri::AttachmentType::Color, VkAttachmentReference{ 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL })
            .setAttachmentReference(hri::AttachmentType::DepthStencil, VkAttachmentReference{ 3, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL })
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
            VkPipelineColorBlendAttachmentState{
                VK_FALSE,
                VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
                VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
                VK_COLOR_COMPONENT_R_BIT
                | VK_COLOR_COMPONENT_G_BIT
                | VK_COLOR_COMPONENT_B_BIT
                | VK_COLOR_COMPONENT_A_BIT
            },
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

        // Set up pipeline layout
        hri::DescriptorSetLayoutBuilder descriptorSetBuilder = hri::DescriptorSetLayoutBuilder(m_pCtx);
        hri::DescriptorSetLayout descriptorSetLayout = descriptorSetBuilder
            .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
            .addBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
            .build();

        hri::PipelineLayoutBuilder layoutBuilder = hri::PipelineLayoutBuilder(m_pCtx);
        VkPipelineLayout pipelineLayout = layoutBuilder
            .addDescriptorSetLayout(descriptorSetLayout)
            .build();

        // Configure graphics pipeline
        hri::GraphicsPipelineBuilder gbufferPipelineBuilder = hri::GraphicsPipelineBuilder{};
        gbufferPipelineBuilder.vertexInputBindings = { VkVertexInputBindingDescription{ 0, sizeof(hri::Vertex), VK_VERTEX_INPUT_RATE_VERTEX } };
        gbufferPipelineBuilder.vertexInputAttributes = {
            VkVertexInputAttributeDescription{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(hri::Vertex, position) },
            VkVertexInputAttributeDescription{ 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(hri::Vertex, normal) },
            VkVertexInputAttributeDescription{ 2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(hri::Vertex, tangent) },
            VkVertexInputAttributeDescription{ 3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(hri::Vertex, textureCoord) },
        };
        gbufferPipelineBuilder.inputAssemblyState = hri::GraphicsPipelineBuilder::initInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, false);
        gbufferPipelineBuilder.viewport = hri::GraphicsPipelineBuilder::initDefaultViewport(1.0f, 1.0f);
        gbufferPipelineBuilder.scissor = hri::GraphicsPipelineBuilder::initDefaultScissor(1, 1);
        gbufferPipelineBuilder.rasterizationState = hri::GraphicsPipelineBuilder::initRasterizationState(false, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
        gbufferPipelineBuilder.multisampleState = hri::GraphicsPipelineBuilder::initMultisampleState(VK_SAMPLE_COUNT_1_BIT);
        gbufferPipelineBuilder.depthStencilState = hri::GraphicsPipelineBuilder::initDepthStencilState(true, true);
        gbufferPipelineBuilder.colorBlendState = hri::GraphicsPipelineBuilder::initColorBlendState(colorBlendAttachments);
        gbufferPipelineBuilder.dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        gbufferPipelineBuilder.layout = pipelineLayout;
        gbufferPipelineBuilder.renderPass = m_renderPass;
        gbufferPipelineBuilder.subpass = 0;

        // Register shaders & create PSO
        shaderDB->registerShader("StaticVert", hri::Shader::loadFile(m_pCtx, "./shaders/static.vert.spv", VK_SHADER_STAGE_VERTEX_BIT));
        shaderDB->registerShader("GBufferLayoutFrag", hri::Shader::loadFile(m_pCtx, "./shaders/gbuffer_layout.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT));
        shaderDB->createPipeline("GBufferLayoutPipeline", { "StaticVert", "GBufferLayoutFrag" }, gbufferPipelineBuilder);
        m_pGbufferPSO = shaderDB->getPipeline("GBufferLayoutPipeline");

        createFrameResources();
    }

    virtual ~GBufferLayoutPass()
    {
        vkDestroyRenderPass(m_pCtx->device, m_renderPass, nullptr);
        destroyFrameResources();
    }

    virtual void createFrameResources() override
    {
        VkExtent2D swapExtent = m_pCtx->swapchain.extent;

        // Set up attachment create infos
        VkImageCreateInfo albedoTargetCreateInfo = VkImageCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        albedoTargetCreateInfo.flags = 0;
        albedoTargetCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        albedoTargetCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        albedoTargetCreateInfo.extent = VkExtent3D{ swapExtent.width, swapExtent.height, 1 };
        albedoTargetCreateInfo.mipLevels = 1;
        albedoTargetCreateInfo.arrayLayers = 1;
        albedoTargetCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        albedoTargetCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        albedoTargetCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        albedoTargetCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        albedoTargetCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkImageCreateInfo wPosTargetCreateInfo = VkImageCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        wPosTargetCreateInfo.flags = 0;
        wPosTargetCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        wPosTargetCreateInfo.format = VK_FORMAT_R16G16B16_SFLOAT;
        wPosTargetCreateInfo.extent = VkExtent3D{ swapExtent.width, swapExtent.height, 1 };
        wPosTargetCreateInfo.mipLevels = 1;
        wPosTargetCreateInfo.arrayLayers = 1;
        wPosTargetCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        wPosTargetCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        wPosTargetCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        wPosTargetCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        wPosTargetCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkImageCreateInfo normalTargetCreateInfo = VkImageCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        normalTargetCreateInfo.flags = 0;
        normalTargetCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        normalTargetCreateInfo.format = VK_FORMAT_R16G16B16_SFLOAT;
        normalTargetCreateInfo.extent = VkExtent3D{ swapExtent.width, swapExtent.height, 1 };
        normalTargetCreateInfo.mipLevels = 1;
        normalTargetCreateInfo.arrayLayers = 1;
        normalTargetCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        normalTargetCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        normalTargetCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        normalTargetCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        normalTargetCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkImageCreateInfo depthTargetCreateInfo = VkImageCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        depthTargetCreateInfo.flags = 0;
        depthTargetCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        depthTargetCreateInfo.format = VK_FORMAT_D32_SFLOAT;
        depthTargetCreateInfo.extent = VkExtent3D{ swapExtent.width, swapExtent.height, 1 };
        depthTargetCreateInfo.mipLevels = 1;
        depthTargetCreateInfo.arrayLayers = 1;
        depthTargetCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        depthTargetCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        depthTargetCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        depthTargetCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        depthTargetCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        // Set up render attachments & views
        m_albedoTarget = hri::RenderAttachment::init(m_pCtx, &albedoTargetCreateInfo);
        m_wPosTarget = hri::RenderAttachment::init(m_pCtx, &wPosTargetCreateInfo);
        m_normalTarget = hri::RenderAttachment::init(m_pCtx, &normalTargetCreateInfo);
        m_depthTarget = hri::RenderAttachment::init(m_pCtx, &depthTargetCreateInfo);

        VkComponentMapping defaultColorMapping = VkComponentMapping{
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
        };

        m_albedoTargetView = m_albedoTarget.createView(m_pCtx, VK_IMAGE_VIEW_TYPE_2D, defaultColorMapping, VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
        m_wPosTargetView = m_wPosTarget.createView(m_pCtx, VK_IMAGE_VIEW_TYPE_2D, defaultColorMapping, VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
        m_normalTargetView = m_normalTarget.createView(m_pCtx, VK_IMAGE_VIEW_TYPE_2D, defaultColorMapping, VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
        m_depthTargetView = m_depthTarget.createView(m_pCtx, VK_IMAGE_VIEW_TYPE_2D, defaultColorMapping, VkImageSubresourceRange{ VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 });

        VkImageView attachments[] = {
            m_albedoTargetView,
            m_wPosTargetView,
            m_normalTargetView,
            m_depthTargetView,
        };

        VkFramebufferCreateInfo fbCreateInfo = VkFramebufferCreateInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        fbCreateInfo.flags = 0;
        fbCreateInfo.renderPass = m_renderPass;
        fbCreateInfo.attachmentCount = HRI_SIZEOF_ARRAY(attachments);
        fbCreateInfo.pAttachments = attachments;
        fbCreateInfo.width = swapExtent.width;
        fbCreateInfo.height = swapExtent.height;
        fbCreateInfo.layers = 1;
        HRI_VK_CHECK(vkCreateFramebuffer(m_pCtx->device, &fbCreateInfo, nullptr, &m_framebuffer));
    }

    virtual void destroyFrameResources() override
    {
        // destroy render attachments & views
        m_albedoTarget.destroyView(m_pCtx, m_albedoTargetView);
        m_wPosTarget.destroyView(m_pCtx, m_wPosTargetView);
        m_normalTarget.destroyView(m_pCtx, m_normalTargetView);
        m_depthTarget.destroyView(m_pCtx, m_depthTargetView);

        hri::RenderAttachment::destroy(m_pCtx, m_albedoTarget);
        hri::RenderAttachment::destroy(m_pCtx, m_wPosTarget);
        hri::RenderAttachment::destroy(m_pCtx, m_normalTarget);
        hri::RenderAttachment::destroy(m_pCtx, m_depthTarget);

        vkDestroyFramebuffer(m_pCtx->device, m_framebuffer, nullptr);
    }

    virtual void setInputAttachment(uint32_t binding, const VkImageView& attachment)
    {
        // TODO: update PSO bindings
    }

    virtual std::vector<VkImageView> getOutputAttachments() const
    {
        return { m_albedoTargetView, m_wPosTargetView, m_normalTargetView, m_depthTargetView };
    }

    virtual void record(const hri::ActiveFrame& frame) const override
    {
        VkExtent2D swapExtent = m_pCtx->swapchain.extent;
        VkClearValue clearValues[] = {
            VkClearValue{{ 0.0f, 0.0f, 0.0f, 1.0f }},   // Albedo target
            VkClearValue{{ 0.0f, 0.0f, 0.0f, 1.0f }},   // wPos target
            VkClearValue{{ 0.0f, 0.0f, 0.0f, 1.0f }},   // Normal target
            VkClearValue{{ 0x00, 1.0f }},               // Depth target
        };

        VkRenderPassBeginInfo passBeginInfo = VkRenderPassBeginInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
        passBeginInfo.renderPass = m_renderPass;
        passBeginInfo.framebuffer = m_framebuffer;
        passBeginInfo.renderArea = VkRect2D{ VkOffset2D{ 0, 0 }, swapExtent };
        passBeginInfo.clearValueCount = HRI_SIZEOF_ARRAY(clearValues);
        passBeginInfo.pClearValues = clearValues;
        vkCmdBeginRenderPass(frame.commandBuffer, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        // TODO: record draw commands (retrive scene data from somewhere?)

        vkCmdEndRenderPass(frame.commandBuffer);
    }

private:
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    hri::RenderAttachment m_albedoTarget = hri::RenderAttachment{};
    hri::RenderAttachment m_wPosTarget = hri::RenderAttachment{};
    hri::RenderAttachment m_normalTarget = hri::RenderAttachment{};
    hri::RenderAttachment m_depthTarget = hri::RenderAttachment{};
    VkImageView m_albedoTargetView = VK_NULL_HANDLE;
    VkImageView m_wPosTargetView = VK_NULL_HANDLE;
    VkImageView m_normalTargetView = VK_NULL_HANDLE;
    VkImageView m_depthTargetView = VK_NULL_HANDLE;
    VkFramebuffer m_framebuffer = VK_NULL_HANDLE;
    hri::PipelineStateObject* m_pGbufferPSO = nullptr;
};

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

        // Set up pipeline layout
        hri::DescriptorSetLayoutBuilder descriptorSetBuilder = hri::DescriptorSetLayoutBuilder(m_pCtx);
        hri::DescriptorSetLayout descriptorSetLayout = descriptorSetBuilder
            .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build();

        hri::PipelineLayoutBuilder layoutBuilder = hri::PipelineLayoutBuilder(m_pCtx);
        VkPipelineLayout pipelineLayout = layoutBuilder
            .addDescriptorSetLayout(descriptorSetLayout)
            .build();

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
        presentPipelineBuilder.layout = pipelineLayout;
        presentPipelineBuilder.renderPass = m_renderPass;
        presentPipelineBuilder.subpass = 0;

        // Register shaders & create PSO
        shaderDB->registerShader("PresentVert", hri::Shader::loadFile(m_pCtx, "shaders/present.vert.spv", VK_SHADER_STAGE_VERTEX_BIT));
        shaderDB->registerShader("PresentFrag", hri::Shader::loadFile(m_pCtx, "shaders/present.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT));
        shaderDB->createPipeline("PresentPipeline", { "PresentVert", "PresentFrag" }, presentPipelineBuilder);
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

    virtual void setInputAttachment(uint32_t binding, const VkImageView& attachment) override
    {
        VkDescriptorImageInfo imageInfo = VkDescriptorImageInfo{};
        imageInfo.sampler = VK_NULL_HANDLE; // TODO: create sampler object for this pass
        imageInfo.imageView = attachment;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet attachmentWriteSet = VkWriteDescriptorSet{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        attachmentWriteSet.dstSet = VK_NULL_HANDLE; // TODO: use allocated descriptor set
        attachmentWriteSet.dstBinding = binding;
        attachmentWriteSet.dstArrayElement = 0;
        attachmentWriteSet.descriptorCount = 1;
        attachmentWriteSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        attachmentWriteSet.pImageInfo = &imageInfo;
        vkUpdateDescriptorSets(m_pCtx->device, 1, &attachmentWriteSet, 0, nullptr);
    }

    virtual std::vector<VkImageView> getOutputAttachments() const
    {
        return {};  // Present pass has no output attachments except swap image
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
    hri::PipelineStateObject* m_pPresentPSO = nullptr;
};
