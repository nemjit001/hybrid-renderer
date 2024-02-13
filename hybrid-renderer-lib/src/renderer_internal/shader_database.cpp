#include "renderer_internal/shader_database.h"

#include <string>
#include <map>
#include <vector>
#include <vulkan/vulkan.h>

#include "renderer_internal/render_context.h"

using namespace hri;

VkPipelineInputAssemblyStateCreateInfo GraphicsPipelineBuilder::initInputAssemblyState(VkPrimitiveTopology topology, VkBool32 primitiveRestart)
{
    return VkPipelineInputAssemblyStateCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        nullptr,
        0,
        topology,
        primitiveRestart,
    };
}

VkViewport GraphicsPipelineBuilder::initDefaultViewport(float width, float height)
{
    return VkViewport{
        0.0f, 0.0f,
        width, height,
        DefaultViewportMinDepth, DefaultViewportMaxDepth,
    };
}

VkRect2D GraphicsPipelineBuilder::initDefaultScissor(uint32_t width, uint32_t height)
{
    return VkRect2D
    {
        VkOffset2D{ 0, 0 },
        VkExtent2D{ width, height },
    };
}

VkPipelineRasterizationStateCreateInfo GraphicsPipelineBuilder::initRasterizationState(VkBool32 discard, VkPolygonMode polygonMode, VkCullModeFlags cullMode, VkFrontFace frontFace)
{
    return VkPipelineRasterizationStateCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        nullptr,
        0,
        VK_FALSE,   // No depth clamp
        discard,
        polygonMode,
        cullMode,
        frontFace,
        VK_FALSE,   // No depth bias
        0.0f, 0.0f, 0.0f, 1.0f, // Fixed defaults
    };
}

VkPipelineDepthStencilStateCreateInfo GraphicsPipelineBuilder::initDepthStencilState(VkBool32 depthTest, VkBool32 depthWrite, VkCompareOp depthCompareOp)
{
    return VkPipelineDepthStencilStateCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        nullptr,
        0,
        depthTest,
        depthWrite,
        depthCompareOp,
        VK_FALSE,   // No depth bounds test
        VK_FALSE,   // No stencil test
        VkStencilOpState{}, VkStencilOpState{}, // Empty defaults
        0.0f, 0.0f  // Empty defaults
    };
}

Shader Shader::loadFile(RenderContext* ctx, const std::string& path, VkShaderStageFlagBits stage)
{
    return Shader::init(ctx, nullptr, 0, stage);
}

Shader Shader::init(RenderContext* ctx, uint32_t* pCode, size_t codeSize, VkShaderStageFlagBits stage)
{
    assert(ctx != nullptr);
    assert(pCode != nullptr && codeSize > 0);

    Shader shader = Shader{};

    VkShaderModuleCreateInfo createInfo = VkShaderModuleCreateInfo{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    createInfo.flags = 0;
    createInfo.codeSize = static_cast<uint32_t>(codeSize);
    createInfo.pCode = pCode;
    HRI_VK_CHECK(vkCreateShaderModule(ctx->device, &createInfo, nullptr, &shader.module));
    
    return shader;
}

void Shader::destroy(RenderContext* ctx, Shader& shader)
{
    assert(ctx != nullptr);

    vkDestroyShaderModule(ctx->device, shader.module, nullptr);

    memset(&shader, 0, sizeof(Shader));
}

ShaderDatabase::ShaderDatabase(RenderContext* ctx)
    :
    m_pCtx(ctx)
{
    assert(m_pCtx != nullptr);

    VkPipelineCacheCreateInfo cacheCreateInfo = VkPipelineCacheCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
    cacheCreateInfo.flags = 0;
    cacheCreateInfo.initialDataSize = 0;
    cacheCreateInfo.pInitialData = nullptr;
    HRI_VK_CHECK(vkCreatePipelineCache(m_pCtx->device, &cacheCreateInfo, nullptr, &m_pipelineCache));
}

ShaderDatabase::~ShaderDatabase()
{
    for (auto& [name, shader] : m_shaderMap)
    {
        Shader::destroy(m_pCtx, shader);
    }

    for (auto& [ name, pso ] : m_pipelineMap)
    {
        vkDestroyPipelineLayout(m_pCtx->device, pso.layout, nullptr);
        vkDestroyPipeline(m_pCtx->device, pso.pipeline, nullptr);
    }

    vkDestroyPipelineCache(m_pCtx->device, m_pipelineCache, nullptr);
}

const Shader* ShaderDatabase::registerShader(const std::string& name, const Shader& shader)
{
    if (isExistingShader(name))
    {
        fprintf(stderr, "Shader [%s] already exists in DB!\n", name.c_str());
        abort();
    }

    const auto& [it, success] = m_shaderMap.insert(std::make_pair(name, shader));
    if (!success)
    {
        fprintf(stderr, "Failed to register Shader [%s] in DB!\n", name.c_str());
        abort();
    }

    return &it->second;
}

const PipelineStateObject* ShaderDatabase::createPipeline(const std::string& name, const std::vector<std::string>& shaders, const GraphicsPipelineBuilder& pipelineBuilder)
{
    if (isExistingPipeline(name))
    {
        fprintf(stderr, "Pipeline [%s] already exists in DB!\n", name.c_str());
        abort();
    }

    // Create pipeline shader stages
    std::vector<VkPipelineShaderStageCreateInfo> pipelineStages;
    pipelineStages.reserve(shaders.size());
    for (auto const& shaderName : shaders)
    {
        const Shader* shader = getShader(shaderName);
        assert(shader != nullptr);

        VkPipelineShaderStageCreateInfo pipelineShaderStage = VkPipelineShaderStageCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        pipelineShaderStage.flags = 0;
        pipelineShaderStage.stage = shader->stage;
        pipelineShaderStage.module = shader->module;
        pipelineShaderStage.pName = "main";
        pipelineShaderStage.pSpecializationInfo = nullptr;
    }

    // Create PSO object
    PipelineStateObject pso = PipelineStateObject{};
    pso.bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    // Create pipeline layout
    // TODO: generate based on shader layout
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = VkPipelineLayoutCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipelineLayoutCreateInfo.flags = 0;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;
    pipelineLayoutCreateInfo.setLayoutCount = 0;
    pipelineLayoutCreateInfo.pSetLayouts = nullptr;
    HRI_VK_CHECK(vkCreatePipelineLayout(m_pCtx->device, &pipelineLayoutCreateInfo, nullptr, &pso.layout));

    // Generate pipeline state from builder
    VkPipelineVertexInputStateCreateInfo vertexInputState = VkPipelineVertexInputStateCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    vertexInputState.flags = 0;
    vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(pipelineBuilder.vertexInputBindings.size());
    vertexInputState.pVertexBindingDescriptions = pipelineBuilder.vertexInputBindings.data();
    vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(pipelineBuilder.vertexInputAttributes.size());
    vertexInputState.pVertexAttributeDescriptions = pipelineBuilder.vertexInputAttributes.data();

    VkPipelineViewportStateCreateInfo viewportState = VkPipelineViewportStateCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    viewportState.flags = 0;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &pipelineBuilder.viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &pipelineBuilder.scissor;

    VkPipelineDynamicStateCreateInfo dynamicState = VkPipelineDynamicStateCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynamicState.flags = 0;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(pipelineBuilder.dynamicStates.size());
    dynamicState.pDynamicStates = pipelineBuilder.dynamicStates.data();

    // Create pipeline
    VkGraphicsPipelineCreateInfo pipelineCreateInfo = VkGraphicsPipelineCreateInfo{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    pipelineCreateInfo.flags = 0;
    pipelineCreateInfo.stageCount = static_cast<uint32_t>(pipelineStages.size());
    pipelineCreateInfo.pStages = pipelineStages.data();
    pipelineCreateInfo.pVertexInputState = &vertexInputState;
    pipelineCreateInfo.pInputAssemblyState = &pipelineBuilder.inputAssemblyState;
    pipelineCreateInfo.pTessellationState = nullptr;
    pipelineCreateInfo.pViewportState = &viewportState;
    pipelineCreateInfo.pRasterizationState = &pipelineBuilder.rasterizationState;
    pipelineCreateInfo.pMultisampleState = nullptr;
    pipelineCreateInfo.pDepthStencilState = &pipelineBuilder.depthStencilState;
    pipelineCreateInfo.pColorBlendState = &pipelineBuilder.colorBlendState;
    pipelineCreateInfo.pDynamicState = &dynamicState;
    pipelineCreateInfo.renderPass = pipelineBuilder.renderPass;
    pipelineCreateInfo.subpass = pipelineBuilder.subpass;
    pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineCreateInfo.basePipelineIndex = 0;
    HRI_VK_CHECK(vkCreateGraphicsPipelines(
        m_pCtx->device,
        m_pipelineCache,
        1,
        &pipelineCreateInfo,
        nullptr,
        &pso.pipeline
    ));

    const auto& [ it, success ] = m_pipelineMap.insert(std::make_pair(name, pso));
    if (!success)
    {
        fprintf(stderr, "Failed to register Pipeline [%s] in DB!\n", name.c_str());
        abort();
    }

    return &it->second;
}

const Shader* ShaderDatabase::getShader(const std::string& name) const
{
    if (!isExistingShader(name))
    {
        fprintf(stderr, "Shader [%s] does not exist in DB!\n", name.c_str());
        abort();
    }

    auto const& it = m_shaderMap.find(name);
    return &it->second;
}

const PipelineStateObject* ShaderDatabase::getPipeline(const std::string& name) const
{
    if (!isExistingPipeline(name))
    {
        fprintf(stderr, "Pipeline [%s] does not exist in DB!\n", name.c_str());
        abort();
    }

    auto const& it = m_pipelineMap.find(name);
    return &it->second;
}

bool ShaderDatabase::isExistingShader(const std::string& name) const
{
    return m_shaderMap.find(name) != m_shaderMap.end();
}

bool ShaderDatabase::isExistingPipeline(const std::string& name) const
{
    return m_pipelineMap.find(name) != m_pipelineMap.end();
}
