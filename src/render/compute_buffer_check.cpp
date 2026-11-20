#include "compute_buffer_check.hpp"

#include "vulkan_resources.hpp"

#include <array>
#include <format>
#include <memory>
#include <utility>

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
constexpr auto compute_stage = vk::ShaderStageFlagBits::eCompute;
} // namespace

ComputeBufferCheck::ComputeBufferCheck(const vk::raii::PhysicalDevice &physical_device, const vk::raii::Device &device)
    : device_(device)
{
    vk::BufferCreateInfo buffer_info;
    buffer_info.size = buffer_size;
    buffer_info.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst;
    buffer_ = vk::raii::Buffer{device_, buffer_info};
    const auto requirements = device_.getBufferMemoryRequirements2({*buffer_}).memoryRequirements;
    const auto properties = physical_device.getMemoryProperties2().memoryProperties;
    const auto type = occt_lab::render::find_memory_type(properties, requirements.memoryTypeBits,
                                                         vk::MemoryPropertyFlagBits::eHostVisible);
    coherent_ =
        static_cast<bool>(properties.memoryTypes[type].propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent);
    vk::MemoryDedicatedAllocateInfo dedicated;
    dedicated.buffer = *buffer_;
    memory_ = vk::raii::DeviceMemory{device_, {requirements.size, type, &dedicated}};
    device_.bindBufferMemory2(vk::BindBufferMemoryInfo{*buffer_, *memory_, 0});

    constexpr vk::DescriptorSetLayoutBinding binding{0, vk::DescriptorType::eStorageBuffer, 1, compute_stage};
    vk::DescriptorSetLayoutCreateInfo set_info;
    set_layout_ = vk::raii::DescriptorSetLayout{device_, set_info.setBindings(binding)};
    constexpr vk::DescriptorPoolSize pool_size{vk::DescriptorType::eStorageBuffer, 1};
    vk::DescriptorPoolCreateInfo pool_info;
    pool_info.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    pool_info.maxSets = 1;
    pool_ = vk::raii::DescriptorPool{device_, pool_info.setPoolSizes(pool_size)};
    vk::DescriptorSetAllocateInfo allocation{*pool_};
    auto sets = device_.allocateDescriptorSets(allocation.setSetLayouts(*set_layout_));
    set_ = std::move(sets.front());
    const vk::DescriptorBufferInfo range{*buffer_, 0, buffer_size};
    vk::WriteDescriptorSet write{*set_, 0, 0, 1, vk::DescriptorType::eStorageBuffer};
    device_.updateDescriptorSets(write.setBufferInfo(range), {});

    constexpr vk::PushConstantRange count_range{compute_stage, 0, sizeof(element_count)};
    vk::PipelineLayoutCreateInfo layout_info;
    layout_info.setSetLayouts(*set_layout_).setPushConstantRanges(count_range);
    layout_ = vk::raii::PipelineLayout{device_, layout_info};
    const auto shader = occt_lab::render::load_shader(device_, "buffer_check.comp.spv");
    const vk::PipelineShaderStageCreateInfo stage{{}, compute_stage, *shader, "computeMain"};
    pipeline_ = vk::raii::Pipeline{device_, nullptr, vk::ComputePipelineCreateInfo{{}, stage, *layout_}};
}

void ComputeBufferCheck::record(const vk::raii::CommandBuffer &commands) const
{
    using Stage = vk::PipelineStageFlagBits2;
    using Access = vk::AccessFlagBits2;
    commands.fillBuffer(*buffer_, 0, buffer_size, sentinel);
    constexpr vk::MemoryBarrier2 before_compute{Stage::eAllTransfer, Access::eTransferWrite, Stage::eComputeShader,
                                                Access::eShaderStorageWrite};
    vk::DependencyInfo dependency;
    commands.pipelineBarrier2(dependency.setMemoryBarriers(before_compute));
    commands.bindPipeline(vk::PipelineBindPoint::eCompute, *pipeline_);
    commands.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *layout_, 0, *set_, {});
    commands.pushConstants<uint32_t>(*layout_, compute_stage, 0, element_count);
    commands.dispatch(group_count, 1, 1);

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
        const auto unmap = [this](void *) { memory_.unmapMemory(); };
        const std::unique_ptr<void, decltype(unmap)> mapped{memory_.mapMemory(0, VK_WHOLE_SIZE), unmap};
        if (!coherent_)
        {
            device_.invalidateMappedMemoryRanges(vk::MappedMemoryRange{*memory_, 0, VK_WHOLE_SIZE});
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
