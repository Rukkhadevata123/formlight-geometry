#include "compute_buffer_check.hpp"

#include <array>
#include <cstring>
#include <format>
#include <memory>

namespace
{
// Dispatch a partial final group. Every inactive lane has a real guard word in the descriptor.
constexpr uint32_t element_count = 65;
constexpr uint32_t local_size = 64;
constexpr uint32_t group_count = (element_count + local_size - 1) / local_size;
constexpr uint32_t word_count = group_count * local_size;
static_assert(element_count < word_count);
constexpr uint32_t sentinel = 0xdeadbeef;
using Readback = std::array<uint32_t, word_count>;
constexpr vk::DeviceSize buffer_size = sizeof(Readback);
} // namespace

ComputeBufferCheck::ComputeBufferCheck(const vk::raii::PhysicalDevice &physical_device, const vk::raii::Device &device)
    : device_(device), kernel_(physical_device, device, "buffer_check.comp.spv", buffer_size,
                               vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eHostVisible)
{
}

void ComputeBufferCheck::record(const vk::raii::CommandBuffer &commands) const
{
    using Stage = vk::PipelineStageFlagBits2;
    using Access = vk::AccessFlagBits2;
    commands.fillBuffer(kernel_.buffer(), 0, buffer_size, sentinel);
    constexpr vk::MemoryBarrier2 before_compute{Stage::eAllTransfer, Access::eTransferWrite, Stage::eComputeShader,
                                                Access::eShaderStorageWrite};
    vk::DependencyInfo dependency;
    commands.pipelineBarrier2(dependency.setMemoryBarriers(before_compute));
    kernel_.dispatch(commands, element_count, group_count);

    // The host checks both the shader results and the guard words written only by fillBuffer.
    constexpr vk::MemoryBarrier2 before_readback{Stage::eComputeShader | Stage::eAllTransfer,
                                                 Access::eShaderStorageWrite | Access::eTransferWrite, Stage::eHost,
                                                 Access::eHostRead};
    commands.pipelineBarrier2(dependency.setMemoryBarriers(before_readback));
}

std::string ComputeBufferCheck::read_result() const
{
    Readback words{};
    {
        const auto unmap = [this](void *) { kernel_.memory().unmapMemory(); };
        const std::unique_ptr<void, decltype(unmap)> mapped{kernel_.memory().mapMemory(0, VK_WHOLE_SIZE), unmap};
        if (!kernel_.host_coherent())
        {
            device_.invalidateMappedMemoryRanges(vk::MappedMemoryRange{*kernel_.memory(), 0, VK_WHOLE_SIZE});
        }
        std::memcpy(words.data(), mapped.get(), sizeof(words));
    }
    for (uint32_t i = 0; i < word_count; ++i)
    {
        if (const uint32_t expected = i < element_count ? 3 * i + 7 : sentinel; words[i] != expected)
        {
            return std::format("Mismatch at word {}: expected {}, read {}", i, expected, words[i]);
        }
    }
    return std::format("Passed: {} values, {} guard words. First = {}, last = {}.", element_count,
                       word_count - element_count, words.front(), words[element_count - 1]);
}
