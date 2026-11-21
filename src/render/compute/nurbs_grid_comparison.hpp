#pragma once

#include "nurbs_grid_input.hpp"
#include "storage_buffer_kernel.hpp"

#include <memory>

// One explicit comparison, retained until its submitting frame fence signals.
class NurbsGridComparison
{
  public:
    NurbsGridComparison(const vk::raii::PhysicalDevice &physical_device, const vk::raii::Device &device,
                        const occt_lab::render::NurbsGridInput &input,
                        std::shared_ptr<const occt_lab::geometry::BSplineSurface> reference);
    void record(const vk::raii::CommandBuffer &commands) const;
    [[nodiscard]] std::string read_report() const;

  private:
    const vk::raii::Device &device_;
    StorageBufferKernel kernel_;
    std::array<std::uint32_t, 6> parameters_;
    occt_lab::geometry::Vec3 origin_;
    double tolerance_mm_;
    std::shared_ptr<const occt_lab::geometry::BSplineSurface> reference_;
};
