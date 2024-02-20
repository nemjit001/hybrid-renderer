#pragma once

#include <map>
#include <vulkan/vulkan.h>

#include "renderer_internal/render_context.h"

#define HRI_MAX_DESCRIPTOR_SET_COUNT    128

namespace hri
{
    /// @brief A Descriptor Set Layout is used to specify bind points for resource descriptors in shaders.
    struct DescriptorSetLayout
    {
        VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
        std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings = {};

        static DescriptorSetLayout init(
            RenderContext *ctx,
            std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings,
            VkDescriptorSetLayoutCreateFlags flags = 0
        );

        static void destroy(RenderContext* ctx, DescriptorSetLayout& layout);
    };

    /// @brief The Descriptor Set Layout Builder allows for efficient building of a descriptor set layout.
    class DescriptorSetLayoutBuilder
    {
    public:
        DescriptorSetLayoutBuilder(RenderContext* ctx);

        virtual ~DescriptorSetLayoutBuilder() = default;

        /// @brief Add a descriptor binding to the descriptor set layout.
        /// @param binding Binding index.
        /// @param type Type of descriptor.
        /// @param shaderStages Shader stages where this descriptor is used.
        /// @param count Number of descriptors in this binding.
        /// @return A reference to this class.
        DescriptorSetLayoutBuilder& addBinding(uint32_t binding, VkDescriptorType type, VkShaderStageFlags shaderStages, uint32_t count = 1);

        /// @brief Set the flags used in descriptor set layout creation.
        /// @param flags Flags to set.
        /// @return A reference to this class.
        DescriptorSetLayoutBuilder& setDescriptorSetFlags(VkDescriptorSetLayoutCreateFlags flags);

        /// @brief Build the specified descriptor set layout.
        /// @return A Descriptor Set Layout.
        DescriptorSetLayout build();

    private:
        RenderContext* m_pCtx = nullptr;
        VkDescriptorSetLayoutCreateFlags m_flags = 0;
        std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> m_bindings = {};
    };

    class DescriptorSetAllocator
    {
    public:
        DescriptorSetAllocator(RenderContext* ctx);

        virtual ~DescriptorSetAllocator();

        DescriptorSetAllocator(const DescriptorSetAllocator&) = delete;
        DescriptorSetAllocator& operator=(const DescriptorSetAllocator&) = delete;

        void allocateDescriptorSet(const DescriptorSetLayout& setlayout, VkDescriptorSet& descriptorSet);

        void freeDescriptorSet(VkDescriptorSet& descriptorSet);

        void reset();

    private:
        void createDescriptorPool(VkDescriptorPool& pool);

    private:
        const std::vector<VkDescriptorPoolSize> c_poolSizes = {
            VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_SAMPLER, HRI_MAX_DESCRIPTOR_SET_COUNT },
            VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, HRI_MAX_DESCRIPTOR_SET_COUNT },
            VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, HRI_MAX_DESCRIPTOR_SET_COUNT },
            VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, HRI_MAX_DESCRIPTOR_SET_COUNT },
            VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, HRI_MAX_DESCRIPTOR_SET_COUNT },
            VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, HRI_MAX_DESCRIPTOR_SET_COUNT },
            VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, HRI_MAX_DESCRIPTOR_SET_COUNT },
            VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, HRI_MAX_DESCRIPTOR_SET_COUNT },
            VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, HRI_MAX_DESCRIPTOR_SET_COUNT },
            VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, HRI_MAX_DESCRIPTOR_SET_COUNT },
            VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, HRI_MAX_DESCRIPTOR_SET_COUNT },
            VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK, HRI_MAX_DESCRIPTOR_SET_COUNT },
            VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, HRI_MAX_DESCRIPTOR_SET_COUNT },
        };

        RenderContext* m_pCtx = nullptr;
        VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    };

    class DescriptorSetManager
    {
    public:
        DescriptorSetManager() = default;

        DescriptorSetManager(RenderContext* ctx, DescriptorSetAllocator* allocator, const DescriptorSetLayout& layout);

        virtual ~DescriptorSetManager();

        DescriptorSetManager(const DescriptorSetManager&) = delete;
        DescriptorSetManager& operator=(const DescriptorSetManager&) = delete;

        DescriptorSetManager(DescriptorSetManager&& other) noexcept;

        DescriptorSetManager& operator=(DescriptorSetManager&& other) noexcept;

        void writeBuffer(uint32_t binding, VkDescriptorBufferInfo* bufferInfo);

        void writeImage(uint32_t binding, VkDescriptorImageInfo* imageInfo);

        void flush();

        inline VkDescriptorSet descriptorSet() const { return m_set; }

    private:
        const VkDescriptorSetLayoutBinding& getLayoutBinding(uint32_t binding) const;

    private:
        RenderContext* m_pCtx = nullptr;
        DescriptorSetAllocator* m_pAllocator = nullptr;
        VkDescriptorSet m_set = VK_NULL_HANDLE;
        std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> m_bindings = {};
        std::vector<VkWriteDescriptorSet> m_writeSets = {};
    };
}
