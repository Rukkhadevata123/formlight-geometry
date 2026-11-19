#pragma once

#include "box_model.hpp"

#include <vulkan/vulkan_raii.hpp>

#include <expected>
#include <memory>
#include <string>

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
    [[nodiscard]] BoxDimensions box_dimensions() const;
    [[nodiscard]] double view_scale(PreviewShape shape) const;
    [[nodiscard]] bool can_undo_box() const;
    [[nodiscard]] bool can_redo_box() const;

    // Finish submitted draws before changing the active box or releasing a history entry.
    [[nodiscard]] std::expected<void, std::string> rebuild_box(BoxDimensions dimensions);
    void undo_box();
    void redo_box();

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

    struct BoxPreview
    {
        BoxModel model;
        MeshBuffer mesh;
        double view_scale;
    };

    [[nodiscard]] std::unique_ptr<BoxPreview> build_box(BoxDimensions dimensions) const;
    [[nodiscard]] MeshBuffer upload_mesh(const BRepMeshData &mesh) const;

    const vk::raii::Device &device_;
    vk::PhysicalDeviceMemoryProperties memory_properties_;
    vk::Format depth_format_;
    std::unique_ptr<BoxPreview> box_;
    // One retained version is available through either Undo or Redo.
    std::unique_ptr<BoxPreview> undo_box_;
    std::unique_ptr<BoxPreview> redo_box_;
    MeshBuffer cylinder_;
    vk::raii::PipelineLayout layout_{nullptr};
    vk::raii::Pipeline pipeline_{nullptr};
};
