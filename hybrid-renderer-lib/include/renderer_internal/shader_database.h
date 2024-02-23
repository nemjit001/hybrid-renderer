#pragma once

#include <string>
#include <map>
#include <vector>
#include <vulkan/vulkan.h>

#include "renderer_internal/render_context.h"
#include "renderer_internal/descriptor_management.h"

#define HRI_SHADER_DB_BUILTIN_NAME(name) ("Builtin::" name)

namespace hri
{
    constexpr float DefaultViewportMinDepth = 0.0f;
    constexpr float DefaultViewportMaxDepth = 1.0f;

    /// @brief The Pipeline Layout Builder allows easy building of pipeline layouts.
    class PipelineLayoutBuilder
    {
    public:
        /// @brief Create a new pipeline layout builder.
        /// @param ctx Render Context to use.
        PipelineLayoutBuilder(RenderContext* ctx);

        /// @brief Destroy this pipeline layout builder.
        virtual ~PipelineLayoutBuilder() = default;

        /// @brief Add a push constant to the pipeline layout.
        /// @param size Size of the push constant to add.
        /// @param shaderStages Shader stages where this constant is active.
        /// @return A reference to this class.
        PipelineLayoutBuilder& addPushConstant(size_t size, VkShaderStageFlags shaderStages);

        /// @brief Add a new Descriptor Set Layout to the pipeline layout.
        /// @param setLayout Desriptor Set Layout to add.
        /// @return A reference to this class.
        PipelineLayoutBuilder& addDescriptorSetLayout(VkDescriptorSetLayout setLayout);

        /// @brief Add a new Descriptor Set Layout to the pipeline layout.
        /// @param setLayout Desriptor Set Layout to add.
        /// @return A reference to this class.
        PipelineLayoutBuilder& addDescriptorSetLayout(const DescriptorSetLayout& setLayout);

        /// @brief Build the configured pipeline layout.
        /// @return A new Pipeline Layout handle.
        VkPipelineLayout build();

    private:
        RenderContext* m_pCtx = nullptr;
        size_t m_pushConstantOffset = 0;
        std::vector<VkPushConstantRange> m_pushConstants = {};
        std::vector<VkDescriptorSetLayout> m_setLayouts = {};
    };

    /// @brief The Graphics Pipeline Builder allows easy configuration & initialization of fixed function state.
    struct GraphicsPipelineBuilder
    {
        std::vector<VkVertexInputBindingDescription> vertexInputBindings;
        std::vector<VkVertexInputAttributeDescription> vertexInputAttributes;
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyState;
        VkViewport viewport;
        VkRect2D scissor;
        VkPipelineRasterizationStateCreateInfo rasterizationState;
        VkPipelineMultisampleStateCreateInfo multisampleState;
        VkPipelineDepthStencilStateCreateInfo depthStencilState;
        VkPipelineColorBlendStateCreateInfo colorBlendState;
        std::vector<VkDynamicState> dynamicStates;
        VkPipelineLayout layout;
        VkRenderPass renderPass;
        uint32_t subpass;

        static VkPipelineInputAssemblyStateCreateInfo initInputAssemblyState(VkPrimitiveTopology topology, VkBool32 primitiveRestart);

        static VkViewport initDefaultViewport(float width, float height);

        static VkRect2D initDefaultScissor(uint32_t width, uint32_t height);

        static VkPipelineRasterizationStateCreateInfo initRasterizationState(VkBool32 discard, VkPolygonMode polygonMode, VkCullModeFlags cullMode, VkFrontFace frontFace);

        static VkPipelineMultisampleStateCreateInfo initMultisampleState(VkSampleCountFlagBits samples);

        static VkPipelineDepthStencilStateCreateInfo initDepthStencilState(VkBool32 depthTest, VkBool32 depthWrite, VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS);

        static VkPipelineColorBlendStateCreateInfo initColorBlendState(const std::vector<VkPipelineColorBlendAttachmentState>& attachments);
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
        static Shader init(RenderContext* ctx, const uint32_t* pCode, size_t codeSize, VkShaderStageFlagBits stage);

        /// @brief Destroy a shader object.
        /// @param ctx Render Context to use.
        /// @param shader Shader to destroy.
        static void destroy(RenderContext* ctx, Shader& shader);
    };

    /// @brief A pipeline state object (PSO) stores a pipeline and its bind point.
    struct PipelineStateObject
    {
        VkPipelineBindPoint bindPoint                   = VK_PIPELINE_BIND_POINT_MAX_ENUM;
        VkPipeline pipeline                             = VK_NULL_HANDLE;
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

        // Disallow copy behaviour
        ShaderDatabase(const ShaderDatabase&) = delete;
        ShaderDatabase& operator=(const ShaderDatabase&) = delete;

        /// @brief Register a Shader in the Shader Database.
        /// @param name Shader name to use. MUST be unique.
        /// @param shader Shader object to register.
        /// @return A pointer to the Shader in the Shader Database.
        Shader* registerShader(const std::string& name, const Shader& shader);

        /// @brief Create a new pipeline object in the Shader Database.
        /// @param name Pipeline name to use. MUST be unique.
        /// @param shaders Shader names to use.
        /// @param pipelineBuilder Pipeline Builder object to use for initialization.
        /// @return A pointer to the Pipeline in the Shader Database.
        PipelineStateObject* createPipeline(
            const std::string& name,
            const std::vector<std::string>& shaders,
            const GraphicsPipelineBuilder& pipelineBuilder
        );

        /// @brief Retrieve a Shader from the Shader Database.
        /// @param name Shader name to retrieve.
        /// @return A pointer to the Shader in the Shader Database.
        Shader* getShader(const std::string& name);

        /// @brief Retrieve a Pipeline from the Shader Database.
        /// @param name Pipeline name to retrieve.
        /// @return A pointer to the Pipeline in the Shader Database.
        PipelineStateObject* getPipeline(const std::string& name);

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
