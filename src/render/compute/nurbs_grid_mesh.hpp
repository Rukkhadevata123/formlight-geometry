#pragma once

#include "application/editable_nurbs_surface.hpp"
#include "storage_buffer_kernel.hpp"

#include <array>
#include <optional>

struct ClipTransform;
class BRepPreview;

class NurbsGridMesh
{
  public:
    NurbsGridMesh(const vk::raii::PhysicalDevice &physical_device, const vk::raii::Device &device,
                  std::uint32_t max_storage_range);
    // Previous frame must have completed before uploading a replacement snapshot.
    void prepare(const EditableNurbsSurface::Snapshot &snapshot, occt_lab::geometry::PatchGrid grid);
    void dispatch_if_needed(const vk::raii::CommandBuffer &commands);
    void draw(const vk::raii::CommandBuffer &commands, const BRepPreview &preview, vk::Extent2D extent,
              const ClipTransform &transform) const;
    [[nodiscard]] std::string read_report() const;

  private:
    const vk::raii::PhysicalDevice &physical_device_;
    const vk::raii::Device &device_;
    std::uint32_t max_storage_range_;
    std::optional<StorageBufferKernel> kernel_;
    vk::DeviceSize capacity_ = 0; // The index buffer begins immediately after the storage descriptor.
    occt_lab::geometry::PatchGrid grid_;
    std::array<std::uint32_t, 6> parameters_{};
    occt_lab::geometry::Vec3 origin_{};
    EditableNurbsSurface::Framing frame_{};
    double tolerance_mm_ = 0;
    bool dispatch_needed_ = false;
    std::shared_ptr<const occt_lab::geometry::BSplineSurface> reference_;
};
