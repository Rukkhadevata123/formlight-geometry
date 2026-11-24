export module occt_lab.render.compute.storage_buffer_kernel;

import std;
import vulkan;
import occt_lab.render.vulkan_resources;

// The kernels share a single storage buffer binding and a small push-constant block.
export class StorageBufferKernel
{
  public:
    StorageBufferKernel(const vk::raii::PhysicalDevice &physical_device, const vk::raii::Device &device,
                        std::string_view shader_file, vk::DeviceSize storage_bytes,
                        vk::BufferUsageFlags additional_usage, vk::MemoryPropertyFlags required_memory,
                        std::uint32_t parameter_bytes = sizeof(std::uint32_t), vk::DeviceSize trailing_bytes = 0);

    void dispatch(const vk::raii::CommandBuffer &commands, const vk::ArrayProxy<const std::uint32_t> &parameters,
                  std::uint32_t groups_x, std::uint32_t groups_y = 1) const;
    [[nodiscard]] vk::Buffer buffer() const
    {
        return *buffer_;
    }
    [[nodiscard]] const vk::raii::DeviceMemory &memory() const
    {
        return memory_;
    }
    [[nodiscard]] bool host_coherent() const
    {
        return host_coherent_;
    }

  private:
    void create_storage_buffer(const vk::raii::PhysicalDevice &physical_device, const vk::raii::Device &device,
                               vk::DeviceSize buffer_bytes, vk::BufferUsageFlags additional_usage,
                               vk::MemoryPropertyFlags required_memory);
    void create_buffer_descriptor(const vk::raii::Device &device, vk::DeviceSize storage_bytes);
    void create_compute_pipeline(const vk::raii::Device &device, std::string_view shader_file,
                                 std::uint32_t parameter_bytes);

    bool host_coherent_ = false;
    vk::raii::DeviceMemory memory_{nullptr};
    vk::raii::Buffer buffer_{nullptr};
    vk::raii::DescriptorSetLayout set_layout_{nullptr};
    vk::raii::DescriptorPool pool_{nullptr};
    vk::raii::DescriptorSet set_{nullptr};
    vk::raii::PipelineLayout layout_{nullptr};
    vk::raii::Pipeline pipeline_{nullptr};
};

namespace
{
constexpr auto compute_stage = vk::ShaderStageFlagBits::eCompute;
}

StorageBufferKernel::StorageBufferKernel(const vk::raii::PhysicalDevice &physical_device,
                                         const vk::raii::Device &device, const std::string_view shader_file,
                                         const vk::DeviceSize storage_bytes,
                                         const vk::BufferUsageFlags additional_usage,
                                         const vk::MemoryPropertyFlags required_memory,
                                         const std::uint32_t parameter_bytes, const vk::DeviceSize trailing_bytes)
{
    // The buffer may have a trailing index region outside the shader's storage descriptor.
    create_storage_buffer(physical_device, device, storage_bytes + trailing_bytes, additional_usage, required_memory);
    create_buffer_descriptor(device, storage_bytes);
    create_compute_pipeline(device, shader_file, parameter_bytes);
}

void StorageBufferKernel::create_storage_buffer(const vk::raii::PhysicalDevice &physical_device,
                                                const vk::raii::Device &device, const vk::DeviceSize buffer_bytes,
                                                const vk::BufferUsageFlags additional_usage,
                                                const vk::MemoryPropertyFlags required_memory)
{
    vk::BufferCreateInfo buffer_info;
    buffer_info.size = buffer_bytes;
    buffer_info.usage = vk::BufferUsageFlagBits::eStorageBuffer | additional_usage;
    buffer_ = vk::raii::Buffer{device, buffer_info};

    const auto requirements = device.getBufferMemoryRequirements2({*buffer_}).memoryRequirements;
    const auto properties = physical_device.getMemoryProperties2().memoryProperties;
    const auto memory_type =
        occt_lab::render::find_memory_type(properties, requirements.memoryTypeBits, required_memory);
    const auto flags = properties.memoryTypes[memory_type].propertyFlags;
    host_coherent_ = static_cast<bool>(flags & vk::MemoryPropertyFlagBits::eHostCoherent);

    vk::MemoryDedicatedAllocateInfo dedicated;
    dedicated.buffer = *buffer_;
    vk::MemoryAllocateInfo allocation;
    allocation.pNext = &dedicated;
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = memory_type;
    memory_ = vk::raii::DeviceMemory{device, allocation};
    device.bindBufferMemory2(vk::BindBufferMemoryInfo{*buffer_, *memory_, 0});
}

void StorageBufferKernel::create_buffer_descriptor(const vk::raii::Device &device, const vk::DeviceSize storage_bytes)
{
    constexpr vk::DescriptorSetLayoutBinding binding{0, vk::DescriptorType::eStorageBuffer, 1, compute_stage};
    vk::DescriptorSetLayoutCreateInfo layout_info;
    layout_info.setBindings(binding);
    set_layout_ = vk::raii::DescriptorSetLayout{device, layout_info};

    constexpr vk::DescriptorPoolSize pool_size{vk::DescriptorType::eStorageBuffer, 1};
    vk::DescriptorPoolCreateInfo pool_info;
    pool_info.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    pool_info.maxSets = 1;
    pool_info.setPoolSizes(pool_size);
    pool_ = vk::raii::DescriptorPool{device, pool_info};

    vk::DescriptorSetAllocateInfo allocation{*pool_};
    allocation.setSetLayouts(*set_layout_);
    auto sets = device.allocateDescriptorSets(allocation);
    set_ = std::move(sets.front());

    const vk::DescriptorBufferInfo range{*buffer_, 0, storage_bytes};
    vk::WriteDescriptorSet write;
    write.dstSet = *set_;
    write.dstBinding = binding.binding;
    write.descriptorType = binding.descriptorType;
    write.setBufferInfo(range);
    device.updateDescriptorSets(write, {});
}

void StorageBufferKernel::create_compute_pipeline(const vk::raii::Device &device, const std::string_view shader_file,
                                                  const std::uint32_t parameter_bytes)
{
    const vk::PushConstantRange parameter_range{compute_stage, 0, parameter_bytes};
    vk::PipelineLayoutCreateInfo layout_info;
    layout_info.setSetLayouts(*set_layout_).setPushConstantRanges(parameter_range);
    layout_ = vk::raii::PipelineLayout{device, layout_info};

    const auto shader = occt_lab::render::load_shader(device, shader_file);
    const vk::PipelineShaderStageCreateInfo stage{{}, compute_stage, *shader, "computeMain"};
    vk::ComputePipelineCreateInfo pipeline_info;
    pipeline_info.stage = stage;
    pipeline_info.layout = *layout_;
    pipeline_ = vk::raii::Pipeline{device, nullptr, pipeline_info};
}

void StorageBufferKernel::dispatch(const vk::raii::CommandBuffer &commands,
                                   const vk::ArrayProxy<const std::uint32_t> &parameters, const std::uint32_t groups_x,
                                   const std::uint32_t groups_y) const
{
    commands.bindPipeline(vk::PipelineBindPoint::eCompute, *pipeline_);
    commands.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *layout_, 0, *set_, {});
    commands.pushConstants<std::uint32_t>(*layout_, compute_stage, 0, parameters);
    commands.dispatch(groups_x, groups_y, 1);
}
