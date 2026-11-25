module;

#include <vulkan/vulkan_core.h>

export module formlight_geometry.render.compute.nurbs_grid_mesh;

import std;
import vulkan;
import formlight_geometry.application.editable_nurbs_surface;
import formlight_geometry.geometry.surfaces.bspline_surface;
import formlight_geometry.render.compute.nurbs_grid_input;
import formlight_geometry.render.compute.nurbs_grid_report;
import formlight_geometry.render.compute.storage_buffer_kernel;
import formlight_geometry.render.preview.brep_preview;
import formlight_geometry.render.viewport.clip_transform;

export class NurbsGridMesh
{
  public:
    NurbsGridMesh(const vk::raii::PhysicalDevice &physical_device, const vk::raii::Device &device,
                  std::uint32_t max_storage_range);
    // Previous frame must have completed before uploading a replacement snapshot.
    void prepare(const EditableNurbsSurface::Snapshot &snapshot, formlight_geometry::geometry::PatchGrid grid);
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
    formlight_geometry::geometry::PatchGrid grid_;
    std::array<std::uint32_t, 6> parameters_{};
    formlight_geometry::geometry::Vec3 origin_{};
    EditableNurbsSurface::Framing frame_{};
    double tolerance_mm_ = 0;
    bool dispatch_needed_ = false;
    std::shared_ptr<const formlight_geometry::geometry::BSplineSurface> reference_;
};

namespace
{
auto map_kernel(const StorageBufferKernel &kernel)
{
    const auto unmap = [&kernel](void *) { kernel.memory().unmapMemory(); };
    return std::unique_ptr<void, decltype(unmap)>{kernel.memory().mapMemory(0, VK_WHOLE_SIZE), unmap};
}
} // namespace

NurbsGridMesh::NurbsGridMesh(const vk::raii::PhysicalDevice &physical_device, const vk::raii::Device &device,
                             const std::uint32_t max_storage_range)
    : physical_device_(physical_device), device_(device), max_storage_range_(max_storage_range)
{
}

void NurbsGridMesh::prepare(const EditableNurbsSurface::Snapshot &snapshot, const formlight_geometry::geometry::PatchGrid grid)
{
    frame_ = snapshot.framing;
    if (reference_ == snapshot.surface && grid_ == grid)
    {
        return;
    }
    const auto input = formlight_geometry::render::pack_nurbs_grid(snapshot.definition, grid, max_storage_range_);
    const vk::DeviceSize bytes = input.words.size() * sizeof(float);
    const bool grow = bytes > capacity_;
    if (grow)
    {
        constexpr auto max_segments = formlight_geometry::geometry::PatchGrid::max_segments;
        constexpr vk::DeviceSize index_bytes = 6 * max_segments * max_segments * sizeof(std::uint32_t);
        const auto capacity = std::min<vk::DeviceSize>(std::bit_ceil(bytes), max_storage_range_ & ~3U);
        kernel_.emplace(physical_device_, device_, "nurbs_grid.comp.spv", capacity,
                        vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer,
                        vk::MemoryPropertyFlagBits::eHostVisible, sizeof(input.parameters), index_bytes);
        capacity_ = capacity;
    }
    const auto mapped = map_kernel(*kernel_);
    std::memcpy(mapped.get(), input.words.data(), input.words.size() * sizeof(float));
    if (grow || grid_ != grid)
    {
        const auto indices = grid.triangle_indices();
        std::memcpy(static_cast<std::byte *>(mapped.get()) + capacity_, indices.data(),
                    indices.size() * sizeof(indices.front()));
    }
    if (!kernel_->host_coherent())
    {
        device_.flushMappedMemoryRanges(vk::MappedMemoryRange{*kernel_->memory(), 0, VK_WHOLE_SIZE});
    }
    parameters_ = input.parameters;
    origin_ = input.origin;
    tolerance_mm_ = std::max(1e-4, 16.0 * std::numeric_limits<float>::epsilon() * input.extent_mm);
    reference_ = snapshot.surface;
    grid_ = grid;
    dispatch_needed_ = true;
}

void NurbsGridMesh::dispatch_if_needed(const vk::raii::CommandBuffer &commands)
{
    if (!dispatch_needed_)
    {
        return;
    }
    // Queue submission makes the flushed host writes available to this dispatch.
    constexpr std::uint32_t group = 8;
    kernel_->dispatch(commands, parameters_, (grid_.u_segments + group) / group, (grid_.v_segments + group) / group);
    constexpr vk::MemoryBarrier2 generated{
        vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite,
        vk::PipelineStageFlagBits2::eVertexAttributeInput | vk::PipelineStageFlagBits2::eHost,
        vk::AccessFlagBits2::eVertexAttributeRead | vk::AccessFlagBits2::eHostRead};
    vk::DependencyInfo dependency;
    commands.pipelineBarrier2(dependency.setMemoryBarriers(generated));
    dispatch_needed_ = false;
}

void NurbsGridMesh::draw(const vk::raii::CommandBuffer &commands, const BRepPreview &preview, const vk::Extent2D extent,
                         const ClipTransform &transform) const
{
    const auto local = transform.from_local_frame(origin_, frame_.center, frame_.world_units_per_view_unit());
    preview.draw_indexed_triangles(commands, extent, local, kernel_->buffer(), capacity_, grid_.index_count(),
                                   PreviewVertexLayout::nurbs_grid);
}

std::string NurbsGridMesh::read_report() const
{
    const auto samples = reference_->sample_grid(grid_);
    if (!samples)
    {
        return std::format("OCCT reference: {}", samples.error());
    }
    const auto mapped = map_kernel(*kernel_);
    if (!kernel_->host_coherent())
    {
        device_.invalidateMappedMemoryRanges(vk::MappedMemoryRange{*kernel_->memory(), 0, VK_WHOLE_SIZE});
    }
    std::vector<formlight_geometry::render::NurbsGridVertex> vertices(samples->size());
    std::memcpy(vertices.data(), mapped.get(), vertices.size() * sizeof(vertices.front()));
    return formlight_geometry::render::compare_nurbs_grid(vertices, *samples, origin_, tolerance_mm_);
}
