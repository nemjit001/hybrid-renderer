#pragma once

#include <string>
#include <map>
#include <vector>
#include <vulkan/vulkan.h>

#include "renderer_internal/render_context.h"

namespace hri
{
    constexpr float DefaultViewportMinDepth = 0.0f;
    constexpr float DefaultViewportMaxDepth = 1.0f;

    /// @brief The Graphics Pipeline Builder allows easy configuration & initialization of fixed function state.
    struct GraphicsPipelineBuilder
    {
        std::vector<VkVertexInputBindingDescription> vertexInputBindings;
        std::vector<VkVertexInputAttributeDescription> vertexInputAttributes;
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyState;
        VkViewport viewport;
        VkRect2D scissor;
        VkPipelineRasterizationStateCreateInfo rasterizationState;
        VkPipelineDepthStencilStateCreateInfo depthStencilState;
        VkPipelineColorBlendStateCreateInfo colorBlendState;    // FIXME: allow easy color blend state registration / initialization
        std::vector<VkDynamicState> dynamicStates;
        VkRenderPass renderPass;
        uint32_t subpass;

        static VkPipelineInputAssemblyStateCreateInfo initInputAssemblyState(VkPrimitiveTopology topology, VkBool32 primitiveRestart);

        static VkViewport initDefaultViewport(float width, float height);

        static VkRect2D initDefaultScissor(uint32_t width, uint32_t height);

        static VkPipelineRasterizationStateCreateInfo initRasterizationState(VkBool32 discard, VkPolygonMode polygonMode, VkCullModeFlags cullMode, VkFrontFace frontFace);

        static VkPipelineDepthStencilStateCreateInfo initDepthStencilState(VkBool32 depthTest, VkBool32 depthWrite, VkCompareOp depthCompareOp);
    };

    /// @brief A Shader object represents a programmable pipeline stage.
    struct Shader
    {
        VkShaderStageFlagBits stage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
        VkShaderModule module       = VK_NULL_HANDLE;

        /// @brief Load a SPIR-V shader file from the filesystem.
        /// @param ctx Render Context to use.
        /// @param path Path to load file from.
        /// @param stage Shader stage flag.
        /// @return A new Shader object.
        static Shader loadFile(RenderContext* ctx, const std::string& path, VkShaderStageFlagBits stage);

        /// @brief Create a new Shader.
        /// @param ctx Render Context to use.
        /// @param pCode SPIR-V bytecode.
        /// @param codeSize Size in bytes for SPIR-V code block.
        /// @param stage Shader stage flag.
        /// @return A new Shader object.
        static Shader init(RenderContext* ctx, uint32_t* pCode, size_t codeSize, VkShaderStageFlagBits stage);

        /// @brief Destroy a shader object.
        /// @param ctx Render Context to use.
        /// @param shader Shader to destroy.
        static void destroy(RenderContext* ctx, Shader& shader);
    };

    /// @brief A pipeline state object (PSO) maintains a pipeline, its layout, and its bind point.
    struct PipelineStateObject
    {
        VkPipelineBindPoint bindPoint   = VK_PIPELINE_BIND_POINT_MAX_ENUM;
        VkPipelineLayout layout         = VK_NULL_HANDLE;
        VkPipeline pipeline             = VK_NULL_HANDLE;
    };

    /// @brief The Shader Database maintains a cache of Shader Modules and PSO's.
    class ShaderDatabase
    {
    public:
        /// @brief Create a new Shader Database.
        /// @param ctx Render Context to use.
        ShaderDatabase(RenderContext* ctx);

        /// @brief Destroy this Shader Database instance.
        virtual ~ShaderDatabase();

        /// @brief Register a Shader in the Shader Database.
        /// @param name Shader name to use. MUST be unique.
        /// @param shader Shader object to register.
        /// @return A pointer to the Shader in the Shader Database.
        const Shader* registerShader(const std::string& name, const Shader& shader);

        /// @brief Create a new pipeline object in the Shader Database.
        /// @param name Pipeline name to use. MUST be unique.
        /// @param shaders Shader names to use.
        /// @param pipelineBuilder Pipeline Builder object to use for initialization.
        /// @return A pointer to the Pipeline in the Shader Database.
        const PipelineStateObject* createPipeline(const std::string& name, const std::vector<std::string>& shaders, const GraphicsPipelineBuilder& pipelineBuilder);

        /// @brief Retrieve a Shader from the Shader Database.
        /// @param name Shader name to retrieve.
        /// @return A pointer to the Shader in the Shader Database.
        const Shader* getShader(const std::string& name) const;

        /// @brief Retrieve a Pipeline from the Shader Database.
        /// @param name Pipeline name to retrieve.
        /// @return A pointer to the Pipeline in the Shader Database.
        const PipelineStateObject* getPipeline(const std::string& name) const;

    private:
        /// @brief Check if a Shader already exists in the Database.
        /// @param name Shader name to check.
        /// @return A boolean to indicate existence.
        bool isExistingShader(const std::string& name) const;

        /// @brief Check if a Pipeline already exists in the Database.
        /// @param name Pipeline name to check.
        /// @return A boolean to indicate existence.
        bool isExistingPipeline(const std::string& name) const;

    private:
        RenderContext* m_pCtx               = nullptr;
        VkPipelineCache m_pipelineCache     = VK_NULL_HANDLE;
        std::map<std::string, Shader> m_shaderMap                   = {};
        std::map<std::string, PipelineStateObject> m_pipelineMap    = {};
    };
}