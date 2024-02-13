#pragma once

#include <string>
#include <map>
#include <vector>
#include <vulkan/vulkan.h>

#include "renderer_internal/render_context.h"

namespace hri
{
    struct GraphicsPipelineBuilder
    {
        //
    };

    struct PipelineStateObject
    {
        VkPipelineBindPoint bindPoint   = VK_PIPELINE_BIND_POINT_MAX_ENUM;
        VkPipelineLayout layout         = VK_NULL_HANDLE;
        VkPipeline pipeline             = VK_NULL_HANDLE;
    };

    class ShaderDatabase
    {
    public:
        ShaderDatabase(RenderContext* ctx);

        virtual ~ShaderDatabase();

        const PipelineStateObject* createPipeline(const std::string& name, const std::vector<std::string>& shaders, GraphicsPipelineBuilder pipelineBuilder);

        const PipelineStateObject* getPipeline(const std::string name) const;

    private:
        bool isExistingPipeline(std::string name) const;

    private:
        RenderContext* m_pCtx = nullptr;
        std::map<std::string, PipelineStateObject> m_pipelineMap = {};
    };
}
