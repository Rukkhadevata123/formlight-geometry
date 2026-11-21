#include "nurbs_grid_comparison.hpp"
#include "nurbs_grid_report.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <format>
#include <limits>
#include <utility>

namespace
{
auto map_kernel(const StorageBufferKernel &kernel)
{
    const auto unmap = [&kernel](void *) { kernel.memory().unmapMemory(); };
    return std::unique_ptr<void, decltype(unmap)>{kernel.memory().mapMemory(0, VK_WHOLE_SIZE), unmap};
}
} // namespace

NurbsGridComparison::NurbsGridComparison(const vk::raii::PhysicalDevice &physical_device,
                                         const vk::raii::Device &device, const occt_lab::render::NurbsGridInput &input,
                                         std::shared_ptr<const occt_lab::geometry::BSplineSurface> reference)
    : device_(device), kernel_(physical_device, device, "nurbs_grid.comp.spv", input.words.size() * sizeof(float), {},
                               vk::MemoryPropertyFlagBits::eHostVisible, sizeof(input.parameters)),
      parameters_(input.parameters), origin_(input.origin),
      tolerance_mm_(std::max(1e-4, 16.0 * std::numeric_limits<float>::epsilon() * input.extent_mm)),
      reference_(std::move(reference))
{
    const auto mapped = map_kernel(kernel_);
    std::memcpy(mapped.get(), input.words.data(), input.words.size() * sizeof(float));
    if (!kernel_.host_coherent())
    {
        device_.flushMappedMemoryRanges(vk::MappedMemoryRange{*kernel_.memory(), 0, VK_WHOLE_SIZE});
    }
}

void NurbsGridComparison::record(const vk::raii::CommandBuffer &commands) const
{
    // Queue submission makes the flushed host writes available to this dispatch.
    constexpr std::uint32_t group = 8;
    kernel_.dispatch(commands, parameters_, (parameters_[0] + group) / group, (parameters_[1] + group) / group);
    constexpr vk::MemoryBarrier2 host_read{vk::PipelineStageFlagBits2::eComputeShader,
                                           vk::AccessFlagBits2::eShaderStorageWrite, vk::PipelineStageFlagBits2::eHost,
                                           vk::AccessFlagBits2::eHostRead};
    vk::DependencyInfo dependency;
    commands.pipelineBarrier2(dependency.setMemoryBarriers(host_read));
}

std::string NurbsGridComparison::read_report() const
{
    const auto samples = reference_->sample_grid({parameters_[0], parameters_[1]});
    if (!samples)
    {
        return std::format("OCCT reference: {}", samples.error());
    }
    const auto mapped = map_kernel(kernel_);
    if (!kernel_.host_coherent())
    {
        device_.invalidateMappedMemoryRanges(vk::MappedMemoryRange{*kernel_.memory(), 0, VK_WHOLE_SIZE});
    }
    std::vector<occt_lab::render::NurbsGridVertex> vertices(samples->size());
    std::memcpy(vertices.data(), mapped.get(), vertices.size() * sizeof(vertices.front()));
    return occt_lab::render::compare_nurbs_grid(vertices, *samples, origin_, tolerance_mm_);
}
