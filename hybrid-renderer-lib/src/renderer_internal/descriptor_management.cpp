#include "renderer_internal/descriptor_management.h"

#include <map>
#include <vulkan/vulkan.h>

#include "renderer_internal/render_context.h"

using namespace hri;

DescriptorSetLayout DescriptorSetLayout::init(
    RenderContext* ctx,
    std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings,
    VkDescriptorSetLayoutCreateFlags flags
)
{
    assert(ctx != nullptr);
    DescriptorSetLayout layout = DescriptorSetLayout{};
    layout.bindings = bindings;

    std::vector<VkDescriptorSetLayoutBinding> setBindings; setBindings.reserve(bindings.size());
    for (auto const& [ bindingIdx, binding ] : bindings)
    {
        setBindings.push_back(binding);
    }

    VkDescriptorSetLayoutCreateInfo createInfo = VkDescriptorSetLayoutCreateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    createInfo.flags = flags;
    createInfo.bindingCount = static_cast<uint32_t>(setBindings.size());
    createInfo.pBindings = setBindings.data();
    HRI_VK_CHECK(vkCreateDescriptorSetLayout(ctx->device, &createInfo, nullptr, &layout.setLayout));

    return layout;
}

void DescriptorSetLayout::destroy(RenderContext* ctx, DescriptorSetLayout& layout)
{
    assert(ctx != nullptr);

    vkDestroyDescriptorSetLayout(ctx->device, layout.setLayout, nullptr);

    layout.setLayout = VK_NULL_HANDLE;
    layout.bindings.clear();
}

DescriptorSetLayoutBuilder::DescriptorSetLayoutBuilder(RenderContext* ctx)
    :
    m_pCtx(ctx)
{
    assert(m_pCtx != nullptr);
}

DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::addBinding(uint32_t binding, VkDescriptorType type, VkShaderStageFlags shaderStages, uint32_t count)
{
    VkDescriptorSetLayoutBinding layoutBinding = VkDescriptorSetLayoutBinding{};
    layoutBinding.binding = binding;
    layoutBinding.descriptorType = type;
    layoutBinding.stageFlags = shaderStages;
    layoutBinding.descriptorCount = count;
    layoutBinding.pImmutableSamplers = nullptr;
    m_bindings.insert_or_assign(binding, layoutBinding);

    return *this;
}

DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::setDescriptorSetFlags(VkDescriptorSetLayoutCreateFlags flags)
{
    m_flags |= flags;

    return *this;
}

DescriptorSetLayout DescriptorSetLayoutBuilder::build()
{
    return DescriptorSetLayout::init(m_pCtx, m_bindings, m_flags);
}

DescriptorAllocator::DescriptorAllocator(RenderContext* ctx)
    :
    m_pCtx(ctx)
{
    assert(m_pCtx != nullptr);

    createDescriptorPool(m_descriptorPool);
}

DescriptorAllocator::~DescriptorAllocator()
{
    vkDestroyDescriptorPool(m_pCtx->device, m_descriptorPool, nullptr);
}

void DescriptorAllocator::allocateDescriptorSet(const DescriptorSetLayout& setlayout, VkDescriptorSet& descriptorSet)
{
    VkDescriptorSetAllocateInfo allocateInfo = VkDescriptorSetAllocateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    allocateInfo.descriptorPool = m_descriptorPool;
    allocateInfo.descriptorSetCount = 1;
    allocateInfo.pSetLayouts = &setlayout.setLayout;

    // FIXME: may fail, handle fail case by allocating new pool & allocating from there
    HRI_VK_CHECK(vkAllocateDescriptorSets(m_pCtx->device, &allocateInfo, &descriptorSet));
}

void DescriptorAllocator::freeDescriptorSet(VkDescriptorSet& descriptorSet)
{
    HRI_VK_CHECK(vkFreeDescriptorSets(m_pCtx->device, m_descriptorPool, 1, &descriptorSet));
    descriptorSet = VK_NULL_HANDLE;
}

void DescriptorAllocator::reset()
{
    vkResetDescriptorPool(m_pCtx->device, m_descriptorPool, 0 /* No reset flags */);
}

void DescriptorAllocator::createDescriptorPool(VkDescriptorPool& pool)
{
    VkDescriptorPoolCreateInfo poolCreateInfo = VkDescriptorPoolCreateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    poolCreateInfo.flags = 0;
    poolCreateInfo.maxSets = HRI_MAX_DESCRIPTOR_SET_COUNT;
    poolCreateInfo.poolSizeCount = static_cast<uint32_t>(c_poolSizes.size());
    poolCreateInfo.pPoolSizes = c_poolSizes.data();
    HRI_VK_CHECK(vkCreateDescriptorPool(m_pCtx->device, &poolCreateInfo, nullptr, &pool));
}
