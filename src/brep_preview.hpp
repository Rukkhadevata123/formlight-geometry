#pragma once

#include <vulkan/vulkan_raii.hpp>

struct BRepMeshData;
struct ClipTransform;

struct PreviewDepthTarget
{
    vk::raii::DeviceMemory memory;
    vk::raii::Image image;
    vk::raii::ImageView view;

    [[nodiscard]] vk::ImageMemoryBarrier2 render_barrier() const;
    [[nodiscard]] vk::RenderingAttachmentInfo attachment() const;
};

enum class PreviewShape
{
    box,
    cylinder,
};

class BRepPreview
{
  public:
    BRepPreview(const vk::raii::PhysicalDevice &physical_device, const vk::raii::Device &device);

    [[nodiscard]] vk::Format depth_format() const;
    [[nodiscard]] PreviewDepthTarget create_depth_target(vk::Extent2D extent) const;
    void create_pipeline(vk::Format color_format);
    void record(const vk::raii::CommandBuffer &commands, vk::Extent2D extent, PreviewShape shape,
                const ClipTransform &transform) const;

  private:
    struct MeshBuffer
    {
        vk::raii::DeviceMemory memory;
        vk::raii::Buffer buffer;
        vk::DeviceSize index_offset;
        uint32_t index_count;
    };

    [[nodiscard]] MeshBuffer upload_mesh(const BRepMeshData &mesh) const;

    const vk::raii::Device &device_;
    vk::PhysicalDeviceMemoryProperties memory_properties_;
    vk::Format depth_format_;
    MeshBuffer box_;
    MeshBuffer cylinder_;
    vk::raii::PipelineLayout layout_{nullptr};
    vk::raii::Pipeline pipeline_{nullptr};
};
