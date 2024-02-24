#include "renderer_internal/shader_database.h"

#include <cstdio>
#include <string>
#include <map>
#include <vector>
#include <vulkan/vulkan.h>

#include "renderer_internal/render_context.h"

using namespace hri;

PipelineLayoutBuilder::PipelineLayoutBuilder(RenderContext& ctx)
    :
    m_ctx(ctx)
{
    //
}

PipelineLayoutBuilder& PipelineLayoutBuilder::addPushConstant(size_t size, VkShaderStageFlags shaderStages)
{
    m_pushConstants.push_back(VkPushConstantRange{
        shaderStages,
        static_cast<uint32_t>(m_pushConstantOffset),
        static_cast<uint32_t>(size),
    });

    m_pushConstantOffset += size;
    return *this;
}

PipelineLayoutBuilder& PipelineLayoutBuilder::addDescriptorSetLayout(VkDescriptorSetLayout setLayout)
{
    m_setLayouts.push_back(setLayout);

    return *this;
}

PipelineLayoutBuilder& PipelineLayoutBuilder::addDescriptorSetLayout(const DescriptorSetLayout& setLayout)
{
    m_setLayouts.push_back(setLayout.setLayout);

    return *this;
}

VkPipelineLayout PipelineLayoutBuilder::build()
{
    VkPipelineLayoutCreateInfo createInfo = VkPipelineLayoutCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    createInfo.flags = 0;
    createInfo.pushConstantRangeCount = static_cast<uint32_t>(m_pushConstants.size());
    createInfo.pPushConstantRanges = m_pushConstants.data();
    createInfo.setLayoutCount = static_cast<uint32_t>(m_setLayouts.size());
    createInfo.pSetLayouts = m_setLayouts.data();

    VkPipelineLayout layout = VK_NULL_HANDLE;
    HRI_VK_CHECK(vkCreatePipelineLayout(m_ctx.device, &createInfo, nullptr, &layout));

    return layout;
}

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

VkPipelineMultisampleStateCreateInfo GraphicsPipelineBuilder::initMultisampleState(VkSampleCountFlagBits samples)
{
    return VkPipelineMultisampleStateCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        nullptr,
        0,
        samples,
        VK_FALSE, 0.0f, // No sample shading
        nullptr,        // No sample mask
        VK_FALSE,       // No alpha to coverage
        VK_FALSE,       // No alpha to one
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

VkPipelineColorBlendStateCreateInfo GraphicsPipelineBuilder::initColorBlendState(const std::vector<VkPipelineColorBlendAttachmentState>& attachments)
{
    return VkPipelineColorBlendStateCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        nullptr,
        0,
        VK_FALSE,
        VK_LOGIC_OP_CLEAR,
        static_cast<uint32_t>(attachments.size()),
        attachments.data(),
        { 0.0f, 0.0f, 0.0f, 0.0f },
    };
}

Shader::Shader(RenderContext& ctx, const uint32_t* pCode, size_t codeSize, VkShaderStageFlagBits stage)
    :
    m_ctx(ctx),
    stage(stage)
{
    assert(pCode != nullptr && codeSize > 0);

    VkShaderModuleCreateInfo createInfo = VkShaderModuleCreateInfo{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    createInfo.flags = 0;
    createInfo.codeSize = static_cast<uint32_t>(codeSize);
    createInfo.pCode = pCode;
    HRI_VK_CHECK(vkCreateShaderModule(m_ctx.device, &createInfo, nullptr, &module));
}

Shader::~Shader()
{
    release();
}

Shader::Shader(Shader&& other) noexcept
    :
    m_ctx(other.m_ctx),
    stage(other.stage),
    module(other.module)
{
    other.module = VK_NULL_HANDLE;
}

Shader& Shader::operator=(Shader&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    release();
    m_ctx = std::move(other.m_ctx);
    stage = other.stage;
    module = other.module;

    other.module = VK_NULL_HANDLE;

    return *this;
}

Shader Shader::loadFile(RenderContext& ctx, const std::string& path, VkShaderStageFlagBits stage)
{
    // Open code file
    FILE* file = fopen(path.c_str(), "rb");
    if (file == nullptr)
    {
        fprintf(stderr, "Failed to open Shader File [%s]\n", path.c_str());
        abort();
    }

    // Check file size
    fseek(file, 0, SEEK_END);
    long codeSize = ftell(file);
    assert(codeSize > 0);

    if (codeSize <= 0)
    {
        fclose(file);
        fprintf(stderr, "Failed to read Shader File [%s]\n", path.c_str());
        abort();
    }

    // Read shader binary blob
    fseek(file, 0, SEEK_SET);
    char* pCode = new char[codeSize + 1] {};
    size_t readBytes = fread(pCode, sizeof(char), codeSize, file);
    assert(readBytes == codeSize);

    // Initialize shader
    Shader shader = Shader(ctx, reinterpret_cast<uint32_t*>(pCode), static_cast<size_t>(readBytes), stage);

    // Free resources
    fclose(file);
    delete[] pCode;

    return shader;
}

void Shader::release()
{
    vkDestroyShaderModule(m_ctx.device, module, nullptr);
}

ShaderDatabase::ShaderDatabase(RenderContext& ctx)
    :
    m_ctx(ctx)
{
    VkPipelineCacheCreateInfo cacheCreateInfo = VkPipelineCacheCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
    cacheCreateInfo.flags = 0;
    cacheCreateInfo.initialDataSize = 0;
    cacheCreateInfo.pInitialData = nullptr;
    HRI_VK_CHECK(vkCreatePipelineCache(m_ctx.device, &cacheCreateInfo, nullptr, &m_pipelineCache));
}

ShaderDatabase::~ShaderDatabase()
{
    for (auto& [ name, pso ] : m_pipelineMap)
    {
        vkDestroyPipeline(m_ctx.device, pso.pipeline, nullptr);
    }

    vkDestroyPipelineCache(m_ctx.device, m_pipelineCache, nullptr);
}

Shader* ShaderDatabase::registerShader(const std::string& name, Shader&& shader)
{
    if (isExistingShader(name))
    {
        fprintf(stderr, "Shader [%s] already exists in DB!\n", name.c_str());
        abort();
    }

    const auto& [it, success] = m_shaderMap.insert(std::pair<std::string, Shader>(name, std::move(shader)));
    if (!success)
    {
        fprintf(stderr, "Failed to register Shader [%s] in DB!\n", name.c_str());
        abort();
    }

    return &it->second;
}

PipelineStateObject* ShaderDatabase::createPipeline(
    const std::string& name,
    const std::vector<std::string>& shaders,
    const GraphicsPipelineBuilder& pipelineBuilder
)
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
        pipelineStages.push_back(pipelineShaderStage);
    }

    // Create PSO object
    PipelineStateObject pso = PipelineStateObject{};
    pso.bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

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
    pipelineCreateInfo.pMultisampleState = &pipelineBuilder.multisampleState;
    pipelineCreateInfo.pDepthStencilState = &pipelineBuilder.depthStencilState;
    pipelineCreateInfo.pColorBlendState = &pipelineBuilder.colorBlendState;
    pipelineCreateInfo.pDynamicState = &dynamicState;
    pipelineCreateInfo.layout = pipelineBuilder.layout;
    pipelineCreateInfo.renderPass = pipelineBuilder.renderPass;
    pipelineCreateInfo.subpass = pipelineBuilder.subpass;
    pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineCreateInfo.basePipelineIndex = 0;
    HRI_VK_CHECK(vkCreateGraphicsPipelines(
        m_ctx.device,
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

PipelineStateObject* ShaderDatabase::createPipeline(
    const std::string& name,
    const std::string& computeShader,
    VkPipelineLayout layout
)
{
    if (isExistingPipeline(name))
    {
        fprintf(stderr, "Pipeline [%s] already exists in DB!\n", name.c_str());
        abort();
    }

    // Get shader
    const Shader* shader = getShader(computeShader);
    assert(shader != nullptr);

    VkPipelineShaderStageCreateInfo shaderStage = VkPipelineShaderStageCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    shaderStage.flags = 0;
    shaderStage.stage = shader->stage;
    shaderStage.module = shader->module;
    shaderStage.pName = "main";
    shaderStage.pSpecializationInfo = nullptr;

    // Create PSO object
    PipelineStateObject pso = PipelineStateObject{};
    pso.bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;

    VkComputePipelineCreateInfo pipelineCreateInfo = VkComputePipelineCreateInfo{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    pipelineCreateInfo.flags = 0;
    pipelineCreateInfo.stage = shaderStage;
    pipelineCreateInfo.layout = layout;
    pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineCreateInfo.basePipelineIndex = 0;
    HRI_VK_CHECK(vkCreateComputePipelines(
        m_ctx.device,
        m_pipelineCache,
        1,
        &pipelineCreateInfo,
        nullptr,
        &pso.pipeline
    ));

    const auto& [it, success] = m_pipelineMap.insert(std::make_pair(name, pso));
    if (!success)
    {
        fprintf(stderr, "Failed to register Pipeline [%s] in DB!\n", name.c_str());
        abort();
    }

    return &it->second;
}

PipelineStateObject* ShaderDatabase::registerPipeline(
    const std::string& name,
    VkPipelineBindPoint bindPoint,
    VkPipeline pipelineHandle
)
{
    assert(pipelineHandle != VK_NULL_HANDLE);

    if (isExistingPipeline(name))
    {
        fprintf(stderr, "Pipeline [%s] already exists in DB!\n", name.c_str());
        abort();
    }

    // Create PSO object
    PipelineStateObject pso = PipelineStateObject{};
    pso.bindPoint = bindPoint;
    pso.pipeline = pipelineHandle;

    const auto& [it, success] = m_pipelineMap.insert(std::make_pair(name, pso));
    if (!success)
    {
        fprintf(stderr, "Failed to register Pipeline [%s] in DB!\n", name.c_str());
        abort();
    }

    return &it->second;
}

Shader* ShaderDatabase::getShader(const std::string& name)
{
    if (!isExistingShader(name))
    {
        fprintf(stderr, "Shader [%s] does not exist in DB!\n", name.c_str());
        abort();
    }

    auto const& it = m_shaderMap.find(name);
    return &it->second;
}

PipelineStateObject* ShaderDatabase::getPipeline(const std::string& name)
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
