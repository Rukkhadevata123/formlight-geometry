module;

#include <SDL3/SDL.h>

export module occt_lab.render.vulkan_resources;

import std;
import vulkan;

namespace occt_lab::render
{
export [[nodiscard]] std::uint32_t find_memory_type(const vk::PhysicalDeviceMemoryProperties &properties,
                                                    const std::uint32_t type_bits,
                                                    const vk::MemoryPropertyFlags required)
{
    for (std::uint32_t index = 0; index < properties.memoryTypeCount; ++index)
    {
        if (const auto flags = properties.memoryTypes[index].propertyFlags;
            (type_bits & (1u << index)) != 0 && (flags & required) == required)
        {
            return index;
        }
    }
    throw std::runtime_error(std::format("No Vulkan memory type satisfies {}", vk::to_string(required)));
}

export [[nodiscard]] vk::raii::ShaderModule load_shader(const vk::raii::Device &device, const std::string_view filename)
{
    const auto *base_path = SDL_GetBasePath();
    if (!base_path)
    {
        throw std::runtime_error(std::format("SDL_GetBasePath: {}", SDL_GetError()));
    }

    const auto path = std::format("{}shaders/{}", base_path, filename);
    std::size_t byte_count = 0;
    const std::unique_ptr<void, decltype(&SDL_free)> bytes{SDL_LoadFile(path.c_str(), &byte_count), SDL_free};
    if (!bytes)
    {
        throw std::runtime_error(std::format("Could not load shader {}: {}", path, SDL_GetError()));
    }
    if (byte_count < 5 * sizeof(std::uint32_t) || byte_count % sizeof(std::uint32_t) != 0)
    {
        throw std::runtime_error(std::format("Invalid SPIR-V size for {}", path));
    }

    std::vector<std::uint32_t> words(byte_count / sizeof(std::uint32_t));
    std::memcpy(words.data(), bytes.get(), byte_count);
    if (words.front() != 0x07230203u)
    {
        throw std::runtime_error(std::format("Invalid SPIR-V header for {}", path));
    }
    vk::ShaderModuleCreateInfo info;
    info.setCode(words);
    return vk::raii::ShaderModule{device, info};
}
} // namespace occt_lab::render
