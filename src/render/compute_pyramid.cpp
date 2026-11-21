#include "compute_pyramid.hpp"

#include "geometry/brep_mesh.hpp"

#include <cstddef>

static_assert(sizeof(MeshVertex) == 24 && offsetof(MeshVertex, normal) == 12);

ComputePyramid::ComputePyramid(const vk::raii::PhysicalDevice &physical_device, const vk::raii::Device &device)
    : kernel_(physical_device, device, "compute_pyramid.comp.spv", vertex_count * sizeof(MeshVertex),
              vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal)
{
}

void ComputePyramid::generate_if_changed(const vk::raii::CommandBuffer &commands, const uint32_t height_mm)
{
    if (generated_height_ == height_mm)
    {
        return;
    }
    // One group of six lanes writes four side triangles and two base triangles.
    kernel_.dispatch(commands, height_mm, 1);

    constexpr vk::MemoryBarrier2 before_draw{
        vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite,
        vk::PipelineStageFlagBits2::eVertexAttributeInput, vk::AccessFlagBits2::eVertexAttributeRead};
    vk::DependencyInfo dependency;
    commands.pipelineBarrier2(dependency.setMemoryBarriers(before_draw));
    generated_height_ = height_mm;
}
