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
        /// @brief Create a new descriptor set layout.
        /// @param ctx Render context to use.
        /// @param bindings Layout Bindings for this descriptor set.
        /// @param flags Create flags.
        /// @return A new Descriptor Set Layout.
        DescriptorSetLayout(
            RenderContext *ctx,
            std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings,
            VkDescriptorSetLayoutCreateFlags flags = 0
        );

        /// @brief Destroy a descriptor set layout.
        virtual ~DescriptorSetLayout();

        DescriptorSetLayout(const DescriptorSetLayout&) = delete;
        DescriptorSetLayout& operator=(const DescriptorSetLayout&) = delete;

        DescriptorSetLayout(DescriptorSetLayout&& other) noexcept;
        DescriptorSetLayout& operator=(DescriptorSetLayout&& other) noexcept;

        /// @brief Retrieve the set layout handle.
        /// @return A vk descriptor set layout handle.
        inline VkDescriptorSetLayout setLayout() const { return m_setLayout; }

        /// @brief Retrieve the bindings stored in this descriptor set layout.
        /// @return A map of bindings.
        inline const std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding>& bindings() const { return m_bindings; }

    private:
        RenderContext* m_pCtx;
        VkDescriptorSetLayout m_setLayout = VK_NULL_HANDLE;
        std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> m_bindings = {};
    };

    /// @brief The Descriptor Set Layout Builder allows for efficient building of a descriptor set layout.
    class DescriptorSetLayoutBuilder
    {
    public:
        /// @brief Create a new builder instance.
        /// @param ctx Render context to use.
        DescriptorSetLayoutBuilder(RenderContext* ctx);

        /// @brief Destroy this builder instance.
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

    /// @brief The Descriptor Set Allocator handles allocation of Descriptor Sets from descriptor pools.
    class DescriptorSetAllocator
    {
    public:
        /// @brief Create a new descriptor set allocator.
        /// @param ctx Render context to use.
        DescriptorSetAllocator(RenderContext* ctx);

        /// @brief Destroy this descriptor set allocator.
        virtual ~DescriptorSetAllocator();

        DescriptorSetAllocator(const DescriptorSetAllocator&) = delete;
        DescriptorSetAllocator& operator=(const DescriptorSetAllocator&) = delete;

        /// @brief Allocate a new descriptor set.
        /// @param setlayout Set layout to use.
        /// @param descriptorSet Descriptor set handle to allocate into.
        void allocateDescriptorSet(const DescriptorSetLayout& setlayout, VkDescriptorSet& descriptorSet);

        /// @brief Free a descriptor set.
        /// @param descriptorSet Descriptor set to free.
        void freeDescriptorSet(VkDescriptorSet& descriptorSet);

        /// @brief Reset the descriptor allocator, invalidating all sets allocated using this allocator.
        void reset();

    private:
        /// @brief Create a new descriptor pool with the default pool sizes.
        /// @param pool Pool handle to store new pool in.
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

    /// @brief A Descriptor Set Manager manages a single descriptor set and its updates.
    class DescriptorSetManager
    {
    public:
        /// @brief Create a new descriptor set manager.
        /// @param ctx Render Context to use.
        /// @param allocator Descriptor Set Allocator to use.
        /// @param layout Descriptor Set Layout to base the managed set on.
        DescriptorSetManager(RenderContext* ctx, DescriptorSetAllocator* allocator, const DescriptorSetLayout& layout);

        /// @brief Destroy this descriptor set manager instance.
        virtual ~DescriptorSetManager();

        /// dissalow copy behaviour.
        DescriptorSetManager(const DescriptorSetManager&) = delete;
        DescriptorSetManager& operator=(const DescriptorSetManager&) = delete;

        // Allow move semantics.
        DescriptorSetManager(DescriptorSetManager&& other) noexcept;
        DescriptorSetManager& operator=(DescriptorSetManager&& other) noexcept;

        /// @brief Queue a buffer write to the descriptor set.
        /// @param binding Binding to write into.
        /// @param bufferInfo Buffer info pointer, must live until the writes are flushed.
        /// @return An instance of this class.
        DescriptorSetManager& writeBuffer(uint32_t binding, VkDescriptorBufferInfo* bufferInfo);

        /// @brief Queue an image write to the descriptor set.
        /// @param binding Binding to write into.
        /// @param imageInfo Image info pointer, must live until the writes are flushed.
        /// @return An instance of this class.
        DescriptorSetManager& writeImage(uint32_t binding, VkDescriptorImageInfo* imageInfo);

        /// @brief Flush queued writes, updating the descriptor set.
        /// @return Am instance of this class.
        DescriptorSetManager& flush();

        /// @brief Retrieve the internal set handle.
        /// @return A vk descriptor set handle.
        inline VkDescriptorSet descriptorSet() const { return m_set; }

    private:
        /// @brief Find a layout binding in the internal bindings map.
        /// @param binding Binding to retrieve.
        /// @return A reference to the binding.
        const VkDescriptorSetLayoutBinding& getLayoutBinding(uint32_t binding) const;

    private:
        RenderContext* m_pCtx = nullptr;
        DescriptorSetAllocator* m_pAllocator = nullptr;
        VkDescriptorSet m_set = VK_NULL_HANDLE;
        std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> m_bindings = {};
        std::vector<VkWriteDescriptorSet> m_writeSets = {};
    };
}
