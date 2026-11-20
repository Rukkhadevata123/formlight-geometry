#pragma once

#include <vulkan/vulkan_raii.hpp>

#include <string>

class ComputeBufferCheck
{
  public:
    ComputeBufferCheck(const vk::raii::PhysicalDevice &physical_device, const vk::raii::Device &device);
    void record(const vk::raii::CommandBuffer &commands) const;
    // Read only after the frame fence for record() has signaled.
    [[nodiscard]] std::string read_result() const;

  private:
    const vk::raii::Device &device_;
    bool coherent_ = false;
    vk::raii::DeviceMemory memory_{nullptr};
    vk::raii::Buffer buffer_{nullptr};
    vk::raii::DescriptorSetLayout set_layout_{nullptr};
    vk::raii::DescriptorPool pool_{nullptr};
    vk::raii::DescriptorSet set_{nullptr};
    vk::raii::PipelineLayout layout_{nullptr};
    vk::raii::Pipeline pipeline_{nullptr};
};
