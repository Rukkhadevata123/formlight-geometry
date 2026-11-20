#pragma once

#include <vulkan/vulkan_raii.hpp>

#include <string_view>

namespace occt_lab::render
{
[[nodiscard]] uint32_t find_memory_type(const vk::PhysicalDeviceMemoryProperties &properties, uint32_t type_bits,
                                        vk::MemoryPropertyFlags required);
[[nodiscard]] vk::raii::ShaderModule load_shader(const vk::raii::Device &device, std::string_view filename);
} // namespace occt_lab::render
