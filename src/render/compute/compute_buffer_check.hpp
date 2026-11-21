#pragma once

#include "storage_buffer_kernel.hpp"

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
    StorageBufferKernel kernel_;
};
