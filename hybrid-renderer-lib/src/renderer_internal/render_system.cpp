#include "renderer_internal/render_system.h"

#include <map>
#include <vulkan/vulkan.h>

#include "renderer_internal/render_context.h"
#include "renderer_internal/render_core.h"
#include "renderer_internal/shader_database.h"

using namespace hri;

RenderSubsystemManager::RenderSubsystemManager(RenderContext* ctx)
    :
    m_pCtx(ctx)
{
    assert(m_pCtx != nullptr);
}

void RenderSubsystemManager::recordFrame(ActiveFrame& frame) const
{
    frame.beginCommands();

    for (auto const& [ name, subsystem ] : m_subsystems)
    {
        // TODO: bind IO descriptor set for pass here
        subsystem->record(frame);
    }

    frame.endCommands();
}

void RenderSubsystemManager::registerSubsystem(const std::string& name, IRenderSubsystem* subsystem, RenderPassIO* subsystemIO)
{
    assert(subsystem != nullptr);
    assert(subsystemIO != nullptr);

    // TODO: fail case handling where name already exists in render system
    m_subsystemIO.insert(std::make_pair(name, subsystemIO));
    m_subsystems.insert(std::make_pair(name, subsystem));
}

IRenderSubsystem::IRenderSubsystem(RenderContext* ctx, ShaderDatabase* shaderDB, VkRenderPass renderPass)
    :
    m_pCtx(ctx),
    m_renderPass(renderPass)
{
    assert(m_pCtx != nullptr);
}
