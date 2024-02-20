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

DescriptorSetAllocator::DescriptorSetAllocator(RenderContext* ctx)
    :
    m_pCtx(ctx)
{
    assert(m_pCtx != nullptr);

    createDescriptorPool(m_descriptorPool);
}

DescriptorSetAllocator::~DescriptorSetAllocator()
{
    vkDestroyDescriptorPool(m_pCtx->device, m_descriptorPool, nullptr);
}

void DescriptorSetAllocator::allocateDescriptorSet(const DescriptorSetLayout& setlayout, VkDescriptorSet& descriptorSet)
{
    VkDescriptorSetAllocateInfo allocateInfo = VkDescriptorSetAllocateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    allocateInfo.descriptorPool = m_descriptorPool;
    allocateInfo.descriptorSetCount = 1;
    allocateInfo.pSetLayouts = &setlayout.setLayout;

    // FIXME: may fail, handle fail case by allocating new pool & allocating from there
    HRI_VK_CHECK(vkAllocateDescriptorSets(m_pCtx->device, &allocateInfo, &descriptorSet));
}

void DescriptorSetAllocator::freeDescriptorSet(VkDescriptorSet& descriptorSet)
{
    HRI_VK_CHECK(vkFreeDescriptorSets(m_pCtx->device, m_descriptorPool, 1, &descriptorSet));
    descriptorSet = VK_NULL_HANDLE;
}

void DescriptorSetAllocator::reset()
{
    vkResetDescriptorPool(m_pCtx->device, m_descriptorPool, 0 /* No reset flags */);
}

void DescriptorSetAllocator::createDescriptorPool(VkDescriptorPool& pool)
{
    VkDescriptorPoolCreateInfo poolCreateInfo = VkDescriptorPoolCreateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    poolCreateInfo.flags = 0;
    poolCreateInfo.maxSets = HRI_MAX_DESCRIPTOR_SET_COUNT;
    poolCreateInfo.poolSizeCount = static_cast<uint32_t>(c_poolSizes.size());
    poolCreateInfo.pPoolSizes = c_poolSizes.data();
    HRI_VK_CHECK(vkCreateDescriptorPool(m_pCtx->device, &poolCreateInfo, nullptr, &pool));
}

DescriptorSetManager::DescriptorSetManager(RenderContext* ctx, DescriptorSetAllocator* allocator, const DescriptorSetLayout& layout)
    :
    m_pCtx(ctx),
    m_pAllocator(allocator),
    m_bindings(layout.bindings)
{
    assert(m_pCtx != nullptr);
    assert(m_pAllocator != nullptr);

    m_pAllocator->allocateDescriptorSet(layout, m_set);
}

DescriptorSetManager::~DescriptorSetManager()
{
    m_pAllocator->freeDescriptorSet(m_set);
}

void DescriptorSetManager::writeBuffer(uint32_t binding, VkDescriptorBufferInfo* bufferInfo)
{
    assert(bufferInfo != nullptr);

    const VkDescriptorSetLayoutBinding& layoutBinding = getLayoutBinding(binding);
    assert(layoutBinding.descriptorCount == 1);

    VkWriteDescriptorSet writeSet = VkWriteDescriptorSet{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    writeSet.dstSet = m_set;
    writeSet.dstBinding = binding;
    writeSet.descriptorCount = layoutBinding.descriptorCount;
    writeSet.descriptorType = layoutBinding.descriptorType;
    writeSet.pBufferInfo = bufferInfo;
}

void DescriptorSetManager::writeImage(uint32_t binding, VkDescriptorImageInfo* imageInfo)
{
    assert(imageInfo != nullptr);

    const VkDescriptorSetLayoutBinding& layoutBinding = getLayoutBinding(binding);
    assert(layoutBinding.descriptorCount == 1);

    VkWriteDescriptorSet writeSet = VkWriteDescriptorSet{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    writeSet.dstSet = m_set;
    writeSet.dstBinding = binding;
    writeSet.descriptorCount = layoutBinding.descriptorCount;
    writeSet.descriptorType = layoutBinding.descriptorType;
    writeSet.pImageInfo = imageInfo;
}

void DescriptorSetManager::flush()
{
    vkUpdateDescriptorSets(
        m_pCtx->device,
        static_cast<uint32_t>(m_writeSets.size()),
        m_writeSets.data(),
        0,
        nullptr
    );

    m_writeSets.clear();
}

const VkDescriptorSetLayoutBinding& DescriptorSetManager::getLayoutBinding(uint32_t binding) const
{
    auto const& it = m_bindings.find(binding);
    assert(it != m_bindings.end());

    return it->second;
}
