#pragma once

#include "geometry/brep/box_model.hpp"
#include "geometry/brep/brep_mesh.hpp"

#include <vulkan/vulkan_raii.hpp>

#include <expected>
#include <memory>
#include <optional>
#include <string>

struct ClipTransform;

struct PreviewDepthTarget
{
    // clang-format off
    vk::raii::DeviceMemory memory;
    vk::raii::Image        image;
    vk::raii::ImageView    view;

    [[nodiscard]] auto render_barrier() const -> vk::ImageMemoryBarrier2;
    [[nodiscard]] auto attachment() const     -> vk::RenderingAttachmentInfo;
    // clang-format on
};

enum class PreviewShape
{
    box,
    cylinder,
};

enum class PreviewVertexLayout { mesh, nurbs_grid };

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
    [[nodiscard]] std::expected<void, std::string> rebuild_box(const BoxDimensions &dimensions);
    void undo_box();
    void redo_box();
    void pick_face(PreviewShape shape, const ClipTransform &transform, double ndc_x, double ndc_y);
    void clear_selection() { selected_face_.reset(); }
    [[nodiscard]] std::optional<std::size_t> selected_face() const { return selected_face_; }

    void create_pipeline(vk::Format color_format);
    void record(const vk::raii::CommandBuffer &commands, vk::Extent2D extent, PreviewShape shape,
                const ClipTransform &transform) const;
    void draw_triangles(const vk::raii::CommandBuffer &commands, vk::Extent2D extent, const ClipTransform &transform,
                        vk::Buffer vertices, uint32_t vertex_count) const;
    void draw_indexed_triangles(const vk::raii::CommandBuffer &commands, vk::Extent2D extent, const ClipTransform &transform,
                                vk::Buffer mesh, vk::DeviceSize index_offset, uint32_t index_count,
                                PreviewVertexLayout vertex_layout = PreviewVertexLayout::mesh) const;

  private:
    void bind_mesh_pipeline(const vk::raii::CommandBuffer &commands, vk::Extent2D extent, const ClipTransform &transform,
                            vk::Buffer vertices, vk::Pipeline pipeline) const;

    struct MeshBuffer
    {
        vk::raii::DeviceMemory memory;
        vk::raii::Buffer buffer;
        vk::DeviceSize index_offset;
        BRepMeshData cpu_mesh;
    };

    struct BoxPreview
    {
        BoxModel model;
        MeshBuffer mesh;
        double view_scale;
    };

    // clang-format off
    [[nodiscard]] auto build_box(const BoxDimensions &dimensions) const -> std::unique_ptr<BoxPreview>;
    [[nodiscard]] auto upload_mesh(BRepMeshData mesh) const             -> MeshBuffer;
    // clang-format on

    const vk::raii::Device &device_;
    vk::PhysicalDeviceMemoryProperties memory_properties_;
    vk::Format depth_format_;
    std::unique_ptr<BoxPreview> box_;
    // One retained version is available through either Undo or Redo.
    std::unique_ptr<BoxPreview> undo_box_;
    std::unique_ptr<BoxPreview> redo_box_;
    MeshBuffer cylinder_;
    std::optional<std::size_t> selected_face_;
    vk::raii::PipelineLayout layout_{nullptr};
    vk::raii::Pipeline pipeline_{nullptr};
    vk::raii::Pipeline nurbs_pipeline_{nullptr};
};
