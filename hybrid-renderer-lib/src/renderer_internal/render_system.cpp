#include "renderer_internal/render_system.h"

#include <map>
#include <vulkan/vulkan.h>

#include "renderer_internal/render_context.h"
#include "renderer_internal/render_core.h"
#include "renderer_internal/shader_database.h"

using namespace hri;

IRenderSubsystem::IRenderSubsystem(RenderContext* ctx, DescriptorSetAllocator* descriptorSetAllocator)
    :
    m_pCtx(ctx),
    m_pDescriptorSetAllocator(descriptorSetAllocator)
{
    assert(m_pCtx != nullptr);
    assert(m_pDescriptorSetAllocator != nullptr);
}

IRenderSubsystem::~IRenderSubsystem()
{
    vkDestroyPipelineLayout(m_pCtx->device, m_layout, nullptr);
}

RenderSubsystemManager::RenderSubsystemManager(RenderContext* ctx)
    :
    m_pCtx(ctx)
{
    assert(m_pCtx != nullptr);
}

void RenderSubsystemManager::recordSubsystem(const std::string& name, ActiveFrame& frame) const
{
    auto const& it = m_subsystems.find(name);
    assert(it != m_subsystems.end());
 
    // TODO: bind subsystem renderpass IO
    hri::IRenderSubsystem* pSubsystem = m_subsystems.at(name);
    assert(pSubsystem != nullptr);

    pSubsystem->record(frame);
}

void RenderSubsystemManager::registerSubsystem(const std::string& name, IRenderSubsystem* subsystem)
{
    assert(subsystem != nullptr);

    // TODO: fail case handling where name already exists in render system
    m_subsystems.insert(std::make_pair(name, subsystem));
}
