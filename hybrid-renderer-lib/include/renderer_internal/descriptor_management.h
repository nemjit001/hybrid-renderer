#pragma once

#include <map>
#include <vulkan/vulkan.h>

#include "renderer_internal/render_context.h"

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
        VkDescriptorSetLayoutCreateFlags m_flags = 0;
        std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> m_bindings = {};
    };

    class DescriptorAllocator
    {
    public:
        DescriptorAllocator(RenderContext* ctx);

        virtual ~DescriptorAllocator();

        void allocateDescriptorSet(const DescriptorSetLayout& setlayout, VkDescriptorSet& descriptorSet);

        void freeDescriptorSet(VkDescriptorSet& descriptorSet);

        void reset();

    private:
        const std::vector<VkDescriptorPoolSize> c_poolSizes = {
            VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_SAMPLER, 256 },
            VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 256 },
            VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 256 },
            VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 256 },
            VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 256 },
            VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 256 },
            VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 256 },
            VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 256 },
            VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 256 },
            VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 256 },
            VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 256 },
            VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK, 256 },
            VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 256 },
        };

        RenderContext* m_pCtx = nullptr;
        VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    };
}
