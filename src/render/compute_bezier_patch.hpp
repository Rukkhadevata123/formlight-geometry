#pragma once

#include "geometry/bezier_patch.hpp"
#include "storage_buffer_kernel.hpp"

#include <optional>
#include <string>

class BRepPreview;
struct ClipTransform;
namespace occt_lab::geometry
{
class OcctBezierPatch;
}

class ComputeBezierPatch
{
  public:
    ComputeBezierPatch(const vk::raii::PhysicalDevice &physical_device, const vk::raii::Device &device);
    void prepare(const vk::raii::CommandBuffer &commands, const occt_lab::geometry::CubicBezierPatch &patch,
                 occt_lab::geometry::PatchGrid grid, bool readback);
    void draw(const vk::raii::CommandBuffer &commands, const BRepPreview &preview, vk::Extent2D extent,
              const ClipTransform &transform) const;
    // Called after the frame which requested readback has finished.
    [[nodiscard]] std::string compare_with_occt(const occt_lab::geometry::OcctBezierPatch &reference) const;

  private:
    const vk::raii::Device &device_;
    StorageBufferKernel kernel_;
    std::optional<occt_lab::geometry::PatchGrid> grid_;
    occt_lab::geometry::CubicBezierPatch::ControlNet poles_{};
};
