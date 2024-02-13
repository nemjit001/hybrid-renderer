#include "renderer_internal/shader_database.h"

#include <string>
#include <map>
#include <vector>
#include <vulkan/vulkan.h>

#include "renderer_internal/render_context.h"

using namespace hri;

ShaderDatabase::ShaderDatabase(RenderContext* ctx)
    :
    m_pCtx(ctx)
{
    assert(m_pCtx != nullptr);
}

ShaderDatabase::~ShaderDatabase()
{
    for (auto const& [ name, pso ] : m_pipelineMap)
    {
        vkDestroyPipelineLayout(m_pCtx->device, pso.layout, nullptr);
        vkDestroyPipeline(m_pCtx->device, pso.pipeline, nullptr);
    }
}

const PipelineStateObject* ShaderDatabase::createPipeline(const std::string& name, const std::vector<std::string>& shaders, GraphicsPipelineBuilder pipelineBuilder)
{
    if (isExistingPipeline(name))
    {
        // FIXME: return error!
    }

    PipelineStateObject pso = PipelineStateObject{};
    pso.bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = VkPipelineLayoutCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipelineLayoutCreateInfo.flags = 0;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;
    pipelineLayoutCreateInfo.setLayoutCount = 0;
    pipelineLayoutCreateInfo.pSetLayouts = nullptr;
    HRI_VK_CHECK(vkCreatePipelineLayout(m_pCtx->device, &pipelineLayoutCreateInfo, nullptr, &pso.layout));

    VkGraphicsPipelineCreateInfo pipelineCreateInfo = VkGraphicsPipelineCreateInfo{};
    pipelineCreateInfo.flags = 0;
    pipelineCreateInfo.stageCount = 0;
    pipelineCreateInfo.pStages = nullptr;
    pipelineCreateInfo.pVertexInputState = nullptr;
    pipelineCreateInfo.pInputAssemblyState = nullptr;
    pipelineCreateInfo.pTessellationState = nullptr;
    pipelineCreateInfo.pViewportState = nullptr;
    pipelineCreateInfo.pMultisampleState = nullptr;
    pipelineCreateInfo.pRasterizationState = nullptr;
    pipelineCreateInfo.pDepthStencilState = nullptr;
    pipelineCreateInfo.pColorBlendState = nullptr;
    pipelineCreateInfo.pDynamicState = nullptr;
    pipelineCreateInfo.renderPass = VK_NULL_HANDLE;
    pipelineCreateInfo.subpass = 0;
    pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineCreateInfo.basePipelineIndex = 0;
    HRI_VK_CHECK(vkCreateGraphicsPipelines(
        m_pCtx->device,
        VK_NULL_HANDLE, // TODO: add pipeline cache support
        1,
        &pipelineCreateInfo,
        nullptr,
        &pso.pipeline
    ));

    const auto& [ it, success ] = m_pipelineMap.insert(std::make_pair(name, pso));
    if (!success)
    {
        // FIXME: return error!
    }

    return &it->second;
}

const PipelineStateObject* ShaderDatabase::getPipeline(const std::string name) const
{
    if (!isExistingPipeline(name))
    {
        // FIXME: return error!
    }

    auto const& it = m_pipelineMap.find(name);
    return &it->second;
}

bool ShaderDatabase::isExistingPipeline(std::string name) const
{
    return m_pipelineMap.find(name) != m_pipelineMap.end();
}
