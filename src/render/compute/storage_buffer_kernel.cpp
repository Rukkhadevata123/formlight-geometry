#include "storage_buffer_kernel.hpp"

#include "render/vulkan_resources.hpp"

#include <utility>

namespace
{
constexpr auto compute_stage = vk::ShaderStageFlagBits::eCompute;
}

StorageBufferKernel::StorageBufferKernel(const vk::raii::PhysicalDevice &physical_device,
                                         const vk::raii::Device &device, const std::string_view shader_file,
                                         const vk::DeviceSize size, const vk::BufferUsageFlags additional_usage,
                                         const vk::MemoryPropertyFlags required_memory, const uint32_t parameter_bytes,
                                         const vk::DeviceSize trailing_bytes)
{
    vk::BufferCreateInfo buffer_info;
    buffer_info.size = size + trailing_bytes;
    buffer_info.usage = vk::BufferUsageFlagBits::eStorageBuffer | additional_usage;
    buffer_ = vk::raii::Buffer{device, buffer_info};
    const auto requirements = device.getBufferMemoryRequirements2({*buffer_}).memoryRequirements;
    const auto properties = physical_device.getMemoryProperties2().memoryProperties;
    const auto type = occt_lab::render::find_memory_type(properties, requirements.memoryTypeBits, required_memory);
    host_coherent_ =
        static_cast<bool>(properties.memoryTypes[type].propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent);
    vk::MemoryDedicatedAllocateInfo dedicated;
    dedicated.buffer = *buffer_;
    memory_ = vk::raii::DeviceMemory{device, {requirements.size, type, &dedicated}};
    device.bindBufferMemory2(vk::BindBufferMemoryInfo{*buffer_, *memory_, 0});

    constexpr vk::DescriptorSetLayoutBinding binding{0, vk::DescriptorType::eStorageBuffer, 1, compute_stage};
    vk::DescriptorSetLayoutCreateInfo set_info;
    set_layout_ = vk::raii::DescriptorSetLayout{device, set_info.setBindings(binding)};
    constexpr vk::DescriptorPoolSize pool_size{vk::DescriptorType::eStorageBuffer, 1};
    vk::DescriptorPoolCreateInfo pool_info;
    pool_info.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    pool_info.maxSets = 1;
    pool_ = vk::raii::DescriptorPool{device, pool_info.setPoolSizes(pool_size)};
    vk::DescriptorSetAllocateInfo allocation{*pool_};
    auto sets = device.allocateDescriptorSets(allocation.setSetLayouts(*set_layout_));
    set_ = std::move(sets.front());
    const vk::DescriptorBufferInfo range{*buffer_, 0, size};
    vk::WriteDescriptorSet write{*set_, 0, 0, 1, vk::DescriptorType::eStorageBuffer};
    device.updateDescriptorSets(write.setBufferInfo(range), {});

    const vk::PushConstantRange parameter_range{compute_stage, 0, parameter_bytes};
    vk::PipelineLayoutCreateInfo layout_info;
    layout_info.setSetLayouts(*set_layout_).setPushConstantRanges(parameter_range);
    layout_ = vk::raii::PipelineLayout{device, layout_info};
    const auto shader = occt_lab::render::load_shader(device, shader_file);
    const vk::PipelineShaderStageCreateInfo stage{{}, compute_stage, *shader, "computeMain"};
    pipeline_ = vk::raii::Pipeline{device, nullptr, vk::ComputePipelineCreateInfo{{}, stage, *layout_}};
}

void StorageBufferKernel::dispatch(const vk::raii::CommandBuffer &commands,
                                   const vk::ArrayProxy<const uint32_t> &parameters, const uint32_t groups_x,
                                   const uint32_t groups_y) const
{
    commands.bindPipeline(vk::PipelineBindPoint::eCompute, *pipeline_);
    commands.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *layout_, 0, *set_, {});
    commands.pushConstants<uint32_t>(*layout_, compute_stage, 0, parameters);
    commands.dispatch(groups_x, groups_y, 1);
}
