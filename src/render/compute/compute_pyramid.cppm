module;

#include <cstddef>

export module formlight_geometry.render.compute.compute_pyramid;

import std;
import vulkan;
import formlight_geometry.geometry.brep.brep_mesh;
import formlight_geometry.render.compute.storage_buffer_kernel;

export class ComputePyramid
{
  public:
    ComputePyramid(const vk::raii::PhysicalDevice &physical_device, const vk::raii::Device &device);

    static constexpr std::uint32_t vertex_count = 6 * 3;
    // Called once per displayed frame, after the previous frame's fence has signaled.
    void generate_if_changed(const vk::raii::CommandBuffer &commands, std::uint32_t height_mm);
    [[nodiscard]] vk::Buffer vertices() const
    {
        return kernel_.buffer();
    }

  private:
    StorageBufferKernel kernel_;
    std::optional<std::uint32_t> generated_height_;
};

static_assert(sizeof(MeshVertex) == 24 && offsetof(MeshVertex, normal) == 12);

ComputePyramid::ComputePyramid(const vk::raii::PhysicalDevice &physical_device, const vk::raii::Device &device)
    : kernel_(physical_device, device, "compute_pyramid.comp.spv", vertex_count * sizeof(MeshVertex),
              vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal)
{
}

void ComputePyramid::generate_if_changed(const vk::raii::CommandBuffer &commands, const std::uint32_t height_mm)
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
