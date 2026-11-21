#include "compute_bezier_patch.hpp"

#include "brep_preview.hpp"
#include "geometry/brep_mesh.hpp"
#include "geometry/occt_bezier_patch.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <format>
#include <memory>
#include <stdexcept>
#include <vector>

namespace
{
using occt_lab::geometry::CubicBezierPatch;
using occt_lab::geometry::OcctBezierPatch;
using occt_lab::geometry::PatchGrid;
using occt_lab::geometry::SurfaceSample;
using occt_lab::geometry::Vec3;
using Stage = vk::PipelineStageFlagBits2;
using Access = vk::AccessFlagBits2;
using Pole = std::array<float, 4>;
constexpr uint32_t max_segments = PatchGrid::max_segments;
constexpr uint32_t max_vertices = (max_segments + 1) * (max_segments + 1);
constexpr vk::DeviceSize index_offset = max_vertices * sizeof(MeshVertex) + 16 * sizeof(Pole);
constexpr vk::DeviceSize max_index_bytes = 6 * max_segments * max_segments * sizeof(uint32_t);
// Acceptance limits for the current millimetre-scale teaching patch.
constexpr double position_tolerance_mm = 1e-4;
constexpr double normal_tolerance_deg = 1e-3;
static_assert(max_index_bytes <= 65536); // vkCmdUpdateBuffer limit
static_assert(sizeof(MeshVertex) == 24 && offsetof(MeshVertex, normal) == 12 && sizeof(Pole) == 16);
static_assert(CubicBezierPatch::side_length == 4);

uint32_t vertex_count(const PatchGrid grid)
{
    return (grid.u_segments + 1) * (grid.v_segments + 1);
}

std::vector<uint32_t> grid_indices(const PatchGrid grid)
{
    std::vector<uint32_t> indices;
    indices.reserve(6 * grid.u_segments * grid.v_segments);
    const auto stride = grid.u_segments + 1;
    for (uint32_t v = 0; v < grid.v_segments; ++v)
    {
        for (uint32_t u = 0; u < grid.u_segments; ++u)
        {
            const auto a = v * stride + u;
            indices.insert(indices.end(), {a, a + 1, a + stride + 1, a, a + stride + 1, a + stride});
        }
    }
    return indices;
}
} // namespace

ComputeBezierPatch::ComputeBezierPatch(const vk::raii::PhysicalDevice &physical_device, const vk::raii::Device &device)
    : device_(device), kernel_(physical_device, device, "bezier_patch.comp.spv", index_offset + max_index_bytes,
                               vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer |
                                   vk::BufferUsageFlagBits::eTransferDst,
                               vk::MemoryPropertyFlagBits::eHostVisible, 2 * sizeof(uint32_t))
{
}

void ComputeBezierPatch::prepare(const vk::raii::CommandBuffer &commands, const CubicBezierPatch &patch,
                                 const PatchGrid grid, const bool readback)
{
    if (grid.u_segments < 1 || grid.v_segments < 1 || grid.u_segments > max_segments || grid.v_segments > max_segments)
    {
        throw std::out_of_range("Bezier grid subdivisions outside supported range");
    }

    vk::DependencyInfo dependency;
    if (grid_ != grid || poles_ != patch.control_net())
    {
        if (grid_ != grid)
        {
            const auto indices = grid_indices(grid);
            commands.updateBuffer<uint32_t>(kernel_.buffer(), index_offset, indices);
        }
        std::array<Pole, 16> input;
        std::ranges::transform(
            patch.control_net(), input.begin(), [](const Vec3 &p) -> Pole
            { return {static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.z), 0.0F}; });
        commands.updateBuffer<Pole>(kernel_.buffer(), vertex_count(grid) * sizeof(MeshVertex), input);
        constexpr vk::MemoryBarrier2 uploaded{
            Stage::eAllTransfer, Access::eTransferWrite, Stage::eComputeShader | Stage::eIndexInput,
            Access::eShaderStorageRead | Access::eShaderStorageWrite | Access::eIndexRead};
        commands.pipelineBarrier2(dependency.setMemoryBarriers(uploaded));
        const std::array subdivisions{grid.u_segments, grid.v_segments};
        constexpr uint32_t group_size = 8;
        kernel_.dispatch(commands, subdivisions, (grid.u_segments + group_size) / group_size,
                         (grid.v_segments + group_size) / group_size);
        constexpr vk::MemoryBarrier2 generated{Stage::eComputeShader, Access::eShaderStorageWrite,
                                               Stage::eVertexAttributeInput | Stage::eIndexInput,
                                               Access::eVertexAttributeRead | Access::eIndexRead};
        commands.pipelineBarrier2(dependency.setMemoryBarriers(generated));
        grid_ = grid;
        poles_ = patch.control_net();
    }
    if (readback)
    {
        constexpr vk::MemoryBarrier2 host_read{Stage::eComputeShader, Access::eShaderStorageWrite, Stage::eHost,
                                               Access::eHostRead};
        commands.pipelineBarrier2(dependency.setMemoryBarriers(host_read));
    }
}

void ComputeBezierPatch::draw(const vk::raii::CommandBuffer &commands, const BRepPreview &preview,
                              const vk::Extent2D extent, const ClipTransform &transform) const
{
    preview.draw_indexed_triangles(commands, extent, transform, kernel_.buffer(), index_offset,
                                   6 * grid_->u_segments * grid_->v_segments);
}

std::string ComputeBezierPatch::compare_with_occt(const OcctBezierPatch &reference) const
{
    const auto grid = grid_.value();
    std::vector<MeshVertex> vertices(vertex_count(grid));
    {
        const auto unmap = [this](void *) { kernel_.memory().unmapMemory(); };
        const std::unique_ptr<void, decltype(unmap)> mapped{kernel_.memory().mapMemory(0, VK_WHOLE_SIZE), unmap};
        if (!kernel_.host_coherent())
        {
            device_.invalidateMappedMemoryRanges(vk::MappedMemoryRange{*kernel_.memory(), 0, VK_WHOLE_SIZE});
        }
        std::memcpy(vertices.data(), mapped.get(), vertices.size() * sizeof(MeshVertex));
    }
    double max_distance = 0;
    double squared_distance = 0;
    double max_angle = 0;
    std::size_t invalid = 0;
    for (uint32_t j = 0; j <= grid.v_segments; ++j)
    {
        for (uint32_t i = 0; i <= grid.u_segments; ++i)
        {
            const auto cpu =
                reference.evaluate(static_cast<double>(i) / grid.u_segments, static_cast<double>(j) / grid.v_segments);
            const auto &[p, n] = vertices[j * (grid.u_segments + 1) + i];
            const double length = std::hypot(n[0], n[1], n[2]);
            if (!cpu || !std::isfinite(length) || std::abs(length - 1.0) > 1e-4 || !std::isfinite(p[0]) ||
                !std::isfinite(p[1]) || !std::isfinite(p[2]))
            {
                ++invalid;
                continue;
            }
            const SurfaceSample gpu{{p[0], p[1], p[2]}, {n[0], n[1], n[2]}};
            const auto [distance, angle] = compare_surface_samples(gpu, *cpu);
            max_distance = std::max(max_distance, distance);
            squared_distance += distance * distance;
            max_angle = std::max(max_angle, angle);
        }
    }
    const auto valid = vertices.size() - invalid;
    if (valid == 0)
    {
        return std::format("Failed: all {} samples invalid.", invalid);
    }
    const bool passed = invalid == 0 && max_distance <= position_tolerance_mm && max_angle <= normal_tolerance_deg;
    return std::format("{}: {} samples, {} invalid\nPosition max / RMS: {:.3g} / {:.3g} mm\nNormal max: {:.3g} deg",
                       passed ? "Passed" : "Failed", vertices.size(), invalid, max_distance,
                       std::sqrt(squared_distance / static_cast<double>(valid)), max_angle);
}
