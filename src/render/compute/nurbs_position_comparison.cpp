#include "nurbs_position_comparison.hpp"

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

NurbsPositionComparison::NurbsPositionComparison(const vk::raii::PhysicalDevice &physical_device,
                                                 const vk::raii::Device &device,
                                                 const occt_lab::render::NurbsPositionInput &input,
                                                 std::shared_ptr<const occt_lab::geometry::BSplineSurface> reference)
    : device_(device), kernel_(physical_device, device, "nurbs_positions.comp.spv", input.words.size() * sizeof(float),
                               {}, vk::MemoryPropertyFlagBits::eHostVisible, sizeof(input.parameters)),
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

void NurbsPositionComparison::record(const vk::raii::CommandBuffer &commands) const
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

std::string NurbsPositionComparison::read_report() const
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
    double max_error = 0.0;
    double squared_error = 0.0;
    for (std::size_t i = 0; i < samples->size(); ++i)
    {
        std::array<float, 4> gpu;
        std::memcpy(gpu.data(), static_cast<const std::byte *>(mapped.get()) + i * sizeof(gpu), sizeof(gpu));
        const auto &[x, y, z] = samples->at(i).position;
        const double distance =
            std::hypot((x - origin_.x) - gpu[0], (y - origin_.y) - gpu[1], (z - origin_.z) - gpu[2]);
        if (!std::isfinite(distance) || !std::isfinite(gpu[3]) || gpu[3] <= 0)
        {
            return std::format("Failed: invalid GPU position or weight at vertex {}.", i);
        }
        max_error = std::max(max_error, distance);
        squared_error += distance * distance;
    }
    return std::format("{}: {} positions\nMax / RMS: {:.4g} / {:.4g} mm\nTolerance: {:g} mm",
                       max_error <= tolerance_mm_ ? "Passed" : "Failed", samples->size(), max_error,
                       std::sqrt(squared_error / static_cast<double>(samples->size())), tolerance_mm_);
}
