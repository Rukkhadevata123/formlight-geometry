#pragma once

#include <vulkan/vulkan_raii.hpp>

#include <string_view>

// The kernels share a single storage buffer binding and a small push-constant block.
class StorageBufferKernel
{
  public:
    StorageBufferKernel(const vk::raii::PhysicalDevice &physical_device, const vk::raii::Device &device,
                        std::string_view shader_file, vk::DeviceSize size, vk::BufferUsageFlags additional_usage,
                        vk::MemoryPropertyFlags required_memory, uint32_t parameter_bytes = sizeof(uint32_t),
                        vk::DeviceSize trailing_bytes = 0);

    void dispatch(const vk::raii::CommandBuffer &commands, const vk::ArrayProxy<const uint32_t> &parameters,
                  uint32_t groups_x, uint32_t groups_y = 1) const;
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
    bool host_coherent_;
    vk::raii::DeviceMemory memory_{nullptr};
    vk::raii::Buffer buffer_{nullptr};
    vk::raii::DescriptorSetLayout set_layout_{nullptr};
    vk::raii::DescriptorPool pool_{nullptr};
    vk::raii::DescriptorSet set_{nullptr};
    vk::raii::PipelineLayout layout_{nullptr};
    vk::raii::Pipeline pipeline_{nullptr};
};
