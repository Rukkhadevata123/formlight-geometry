#pragma once

#include "storage_buffer_kernel.hpp"

#include <optional>

class ComputePyramid
{
  public:
    ComputePyramid(const vk::raii::PhysicalDevice &physical_device, const vk::raii::Device &device);

    static constexpr uint32_t vertex_count = 6 * 3;
    // Called once per displayed frame, after the previous frame's fence has signaled.
    void generate_if_changed(const vk::raii::CommandBuffer &commands, uint32_t height_mm);
    [[nodiscard]] vk::Buffer vertices() const
    {
        return kernel_.buffer();
    }

  private:
    StorageBufferKernel kernel_;
    std::optional<uint32_t> generated_height_;
};
