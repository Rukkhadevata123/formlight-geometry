module;

#include <BRepPrimAPI_MakeCylinder.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Solid.hxx>
#include <cstddef>
#include <gp_Pnt.hxx>
#include <vulkan/vulkan_raii.hpp>

export module occt_lab.render.preview.brep_preview;

import std;
import occt_lab.geometry.brep.box_model;
import occt_lab.geometry.brep.brep_face_properties;
import occt_lab.geometry.brep.brep_mesh;
import occt_lab.geometry.brep.plate_with_hole;
import occt_lab.render.compute.nurbs_grid_input;
import occt_lab.render.viewport.brep_face_picking;
import occt_lab.render.viewport.clip_transform;
import occt_lab.render.viewport.orbit_camera;
import occt_lab.render.vulkan_resources;

export struct PreviewDepthTarget
{
    // clang-format off
    vk::raii::DeviceMemory memory;
    vk::raii::Image        image;
    vk::raii::ImageView    view;

    [[nodiscard]] auto render_barrier() const -> vk::ImageMemoryBarrier2;
    [[nodiscard]] auto attachment() const     -> vk::RenderingAttachmentInfo;
    // clang-format on
};

export enum class PreviewShape
{
    box,
    cylinder,
    plate_with_hole_cut,
    plate_with_hole_extrusion,
};

export enum class PreviewVertexLayout { mesh, nurbs_grid };

export class BRepPreview
{
  public:
    BRepPreview(const vk::raii::PhysicalDevice &physical_device, const vk::raii::Device &device);

    [[nodiscard]] vk::Format depth_format() const;
    [[nodiscard]] PreviewDepthTarget create_depth_target(vk::Extent2D extent) const;
    [[nodiscard]] BoxDimensions box_dimensions() const;
    [[nodiscard]] const TopoDS_Solid &source_solid(PreviewShape shape) const;
    [[nodiscard]] double view_scale(PreviewShape shape) const;
    [[nodiscard]] bool can_undo_box() const;
    [[nodiscard]] bool can_redo_box() const;

    // Finish submitted draws before changing the active box or releasing a history entry.
    [[nodiscard]] std::expected<void, std::string> rebuild_box(const BoxDimensions &dimensions);
    void undo_box();
    void redo_box();
    void pick_face(PreviewShape shape, const ClipTransform &transform, double ndc_x, double ndc_y);
    void clear_selection() { selected_face_.reset(); }
    [[nodiscard]] std::optional<BRepFaceProperties> selected_face() const { return selected_face_; }

    void create_pipeline(vk::Format color_format);
    void record(const vk::raii::CommandBuffer &commands, vk::Extent2D extent, PreviewShape shape,
                const ClipTransform &transform) const;
    void draw_triangles(const vk::raii::CommandBuffer &commands, vk::Extent2D extent, const ClipTransform &transform,
                        vk::Buffer vertices, std::uint32_t vertex_count) const;
    void draw_indexed_triangles(const vk::raii::CommandBuffer &commands, vk::Extent2D extent, const ClipTransform &transform,
                                vk::Buffer mesh, vk::DeviceSize index_offset, std::uint32_t index_count,
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
    [[nodiscard]] auto mesh_for(PreviewShape shape) const              -> const MeshBuffer &;
    // clang-format on

    const vk::raii::Device &device_;
    vk::PhysicalDeviceMemoryProperties memory_properties_;
    vk::Format depth_format_;
    std::unique_ptr<BoxPreview> box_;
    // One retained version is available through either Undo or Redo.
    std::unique_ptr<BoxPreview> undo_box_;
    std::unique_ptr<BoxPreview> redo_box_;
    MeshBuffer cylinder_;
    MeshBuffer plate_with_hole_cut_;
    MeshBuffer plate_with_hole_extrusion_;
    std::optional<BRepFaceProperties> selected_face_;
    vk::raii::PipelineLayout layout_{nullptr};
    vk::raii::Pipeline pipeline_{nullptr};
    vk::raii::Pipeline nurbs_pipeline_{nullptr};
};

namespace
{
using occt_lab::render::find_memory_type;
using occt_lab::render::load_shader;
constexpr vk::ImageSubresourceRange depth_range{vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1};
constexpr auto depth_layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
constexpr double linear_deflection = 0.1;
constexpr BoxDimensions plate_dimensions{.length = 100.0, .width = 80.0, .height = 20.0};
constexpr double plate_hole_radius = 20.0;
constexpr auto mesh_stages = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

static_assert(sizeof(ClipTransform) == 64 && offsetof(ClipTransform, w) == 48);
static_assert(offsetof(MeshVertex, normal) == 12 && sizeof(MeshVertex) == 24);

BRepMeshData make_cylinder_mesh()
{
    constexpr double radius = 40.0;
    constexpr double height = 60.0;
    BRepPrimAPI_MakeCylinder cylinder{radius, height};
    constexpr gp_Pnt center{0.0, 0.0, 0.5 * height};
    return triangulate_brep(cylinder.Shape(), center, linear_deflection);
}

BRepMeshData make_plate_mesh(const TopoDS_Solid &plate)
{
    constexpr gp_Pnt center{0.5 * plate_dimensions.length, 0.5 * plate_dimensions.width, 0.5 * plate_dimensions.height};
    return triangulate_brep(plate, center, linear_deflection);
}

vk::Format choose_depth_format(const vk::raii::PhysicalDevice &physical_device)
{
    for (const auto format : {vk::Format::eD32Sfloat, vk::Format::eD16Unorm})
    {
        if (const auto features = physical_device.getFormatProperties2(format).formatProperties.optimalTilingFeatures;
            features & vk::FormatFeatureFlagBits::eDepthStencilAttachment)
        {
            return format;
        }
    }
    throw std::runtime_error("No supported depth attachment format for the B-Rep preview");
}

} // namespace

vk::ImageMemoryBarrier2 PreviewDepthTarget::render_barrier() const
{
    constexpr auto stages =
        vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests;
    constexpr auto depth_read = vk::AccessFlagBits2::eDepthStencilAttachmentRead;
    constexpr auto depth_write = vk::AccessFlagBits2::eDepthStencilAttachmentWrite;

    vk::ImageMemoryBarrier2 barrier;
    barrier.srcStageMask = stages;
    barrier.srcAccessMask = depth_write;
    barrier.dstStageMask = stages;
    barrier.dstAccessMask = depth_read | depth_write;
    barrier.oldLayout = vk::ImageLayout::eUndefined;
    barrier.newLayout = depth_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = *image;
    barrier.subresourceRange = depth_range;
    return barrier;
}

vk::RenderingAttachmentInfo PreviewDepthTarget::attachment() const
{
    vk::RenderingAttachmentInfo info;
    info.imageView = *view;
    info.imageLayout = depth_layout;
    info.loadOp = vk::AttachmentLoadOp::eClear;
    info.storeOp = vk::AttachmentStoreOp::eDontCare;
    info.clearValue = vk::ClearDepthStencilValue{1.0F, 0};
    return info;
}

BRepPreview::BRepPreview(const vk::raii::PhysicalDevice &physical_device, const vk::raii::Device &device)
    : device_(device), memory_properties_(physical_device.getMemoryProperties2().memoryProperties),
      depth_format_(choose_depth_format(physical_device)), box_(build_box(BoxDimensions{})),
      cylinder_(upload_mesh(make_cylinder_mesh())),
      plate_with_hole_cut_(upload_mesh(make_plate_mesh(cut_plate_with_hole(plate_dimensions, plate_hole_radius)))),
      plate_with_hole_extrusion_(upload_mesh(make_plate_mesh(extrude_plate_with_hole(plate_dimensions, plate_hole_radius))))
{
    constexpr vk::PushConstantRange mesh_parameters{mesh_stages, 0, sizeof(ClipTransform) + sizeof(std::uint32_t)};
    vk::PipelineLayoutCreateInfo info;
    info.setPushConstantRanges(mesh_parameters);
    layout_ = vk::raii::PipelineLayout{device_, info};
}

auto BRepPreview::build_box(const BoxDimensions &dimensions) const -> std::unique_ptr<BoxPreview>
{
    auto model = make_box(dimensions);
    const auto [length, width, height] = model.dimensions;
    const gp_Pnt center{0.5 * length, 0.5 * width, 0.5 * height};
    auto mesh = triangulate_brep(model.shape, center, linear_deflection);

    // Keep comparable boxes at the same scale and adjust framing for sizes outside that range.
    constexpr BoxDimensions defaults;
    const double default_diagonal = std::hypot(defaults.length, defaults.width, defaults.height);
    const double diagonal = std::hypot(length, width, height);
    const double view_diagonal = std::clamp(diagonal, 0.25 * default_diagonal, 1.125 * default_diagonal);
    return std::make_unique<BoxPreview>(BoxPreview{
        .model = std::move(model),
        .mesh = upload_mesh(std::move(mesh)),
        .view_scale = diagonal / view_diagonal,
    });
}

BoxDimensions BRepPreview::box_dimensions() const
{
    return box_->model.dimensions;
}

const TopoDS_Solid &BRepPreview::source_solid(const PreviewShape shape) const
{
    return TopoDS::Solid(mesh_for(shape).cpu_mesh.source_shape);
}

double BRepPreview::view_scale(const PreviewShape shape) const
{
    return shape == PreviewShape::box ? box_->view_scale : 1.0;
}

bool BRepPreview::can_undo_box() const
{
    return undo_box_ != nullptr;
}

bool BRepPreview::can_redo_box() const
{
    return redo_box_ != nullptr;
}

std::expected<void, std::string> BRepPreview::rebuild_box(const BoxDimensions &dimensions)
{
    if (dimensions == box_dimensions())
    {
        return {};
    }

    try
    {
        auto replacement = build_box(dimensions);
        undo_box_ = std::exchange(box_, std::move(replacement));
        redo_box_.reset();
        clear_selection();
        return {};
    }
    catch (const vk::DeviceLostError &)
    {
        throw;
    }
    catch (const std::exception &error)
    {
        const std::string_view message{error.what()};
        return std::unexpected(message.empty() ? std::string{"Unknown error"} : std::string{message});
    }
}

void BRepPreview::undo_box()
{
    if (undo_box_)
    {
        redo_box_ = std::exchange(box_, std::move(undo_box_));
        clear_selection();
    }
}

void BRepPreview::redo_box()
{
    if (redo_box_)
    {
        undo_box_ = std::exchange(box_, std::move(redo_box_));
        clear_selection();
    }
}

void BRepPreview::pick_face(const PreviewShape shape, const ClipTransform &transform, const double ndc_x, const double ndc_y)
{
    const auto &mesh = mesh_for(shape);
    const auto face_index = pick_brep_face(mesh.cpu_mesh, transform, ndc_x, ndc_y);
    if (!face_index)
    {
        clear_selection();
        return;
    }
    selected_face_ = inspect_brep_face(mesh.cpu_mesh, *face_index);
}

auto BRepPreview::mesh_for(const PreviewShape shape) const -> const MeshBuffer &
{
    switch (shape)
    {
    case PreviewShape::box:
        return box_->mesh;
    case PreviewShape::cylinder:
        return cylinder_;
    case PreviewShape::plate_with_hole_cut:
        return plate_with_hole_cut_;
    case PreviewShape::plate_with_hole_extrusion:
        return plate_with_hole_extrusion_;
    }
    throw std::logic_error("Unknown B-Rep preview shape");
}

auto BRepPreview::upload_mesh(BRepMeshData mesh) const -> MeshBuffer
{
    const auto vertices = std::as_bytes(std::span{mesh.vertices});
    const auto indices = std::as_bytes(std::span{mesh.indices});

    vk::BufferCreateInfo info;
    info.size = vertices.size() + indices.size();
    info.usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer;
    MeshBuffer buffers{
        .memory = vk::raii::DeviceMemory{nullptr},
        .buffer = vk::raii::Buffer{device_, info},
        .index_offset = vertices.size(),
    };

    const auto requirements = device_.getBufferMemoryRequirements2({*buffers.buffer}).memoryRequirements;
    const auto memory_type =
        find_memory_type(memory_properties_, requirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible);
    vk::MemoryDedicatedAllocateInfo dedicated;
    dedicated.buffer = *buffers.buffer;
    buffers.memory = vk::raii::DeviceMemory{device_, {requirements.size, memory_type, &dedicated}};
    device_.bindBufferMemory2(vk::BindBufferMemoryInfo{*buffers.buffer, *buffers.memory, 0});
    {
        const auto unmap = [&buffers](void *) { buffers.memory.unmapMemory(); };
        const std::unique_ptr<void, decltype(unmap)> mapped{buffers.memory.mapMemory(0, VK_WHOLE_SIZE), unmap};
        auto *destination = static_cast<std::byte *>(mapped.get());
        std::memcpy(destination, vertices.data(), vertices.size());
        std::memcpy(destination + vertices.size(), indices.data(), indices.size());
        if (!(memory_properties_.memoryTypes[memory_type].propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent))
        {
            device_.flushMappedMemoryRanges(vk::MappedMemoryRange{*buffers.memory, 0, VK_WHOLE_SIZE});
        }
    }
    buffers.cpu_mesh = std::move(mesh);
    return buffers;
}

vk::Format BRepPreview::depth_format() const
{
    return depth_format_;
}

PreviewDepthTarget BRepPreview::create_depth_target(const vk::Extent2D extent) const
{
    vk::ImageCreateInfo info;
    info.imageType = vk::ImageType::e2D;
    info.format = depth_format_;
    info.extent = vk::Extent3D{extent.width, extent.height, 1};
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = vk::SampleCountFlagBits::e1;
    info.tiling = vk::ImageTiling::eOptimal;
    info.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;

    PreviewDepthTarget target{
        .memory = vk::raii::DeviceMemory{nullptr},
        .image = vk::raii::Image{device_, info},
        .view = vk::raii::ImageView{nullptr},
    };
    const auto requirements = device_.getImageMemoryRequirements2({*target.image}).memoryRequirements;
    const auto memory_type =
        find_memory_type(memory_properties_, requirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
    vk::MemoryDedicatedAllocateInfo dedicated;
    dedicated.image = *target.image;
    target.memory = vk::raii::DeviceMemory{device_, {requirements.size, memory_type, &dedicated}};
    device_.bindImageMemory2(vk::BindImageMemoryInfo{*target.image, *target.memory, 0});
    target.view =
        vk::raii::ImageView{device_, {{}, *target.image, vk::ImageViewType::e2D, depth_format_, {}, depth_range}};
    return target;
}

void BRepPreview::create_pipeline(const vk::Format color_format)
{
    const auto vertex_shader = load_shader(device_, "brep_preview.vert.spv");
    const auto fragment_shader = load_shader(device_, "brep_preview.frag.spv");
    const std::array stages{
        vk::PipelineShaderStageCreateInfo{{}, vk::ShaderStageFlagBits::eVertex, *vertex_shader, "vertexMain"},
        vk::PipelineShaderStageCreateInfo{{}, vk::ShaderStageFlagBits::eFragment, *fragment_shader, "fragmentMain"},
    };

    vk::VertexInputBindingDescription binding{0, sizeof(MeshVertex), vk::VertexInputRate::eVertex};
    std::array attributes{
        vk::VertexInputAttributeDescription{0, 0, vk::Format::eR32G32B32Sfloat, offsetof(MeshVertex, position)},
        vk::VertexInputAttributeDescription{1, 0, vk::Format::eR32G32B32Sfloat, offsetof(MeshVertex, normal)},
    };
    vk::PipelineVertexInputStateCreateInfo vertex_input;
    vertex_input.setVertexBindingDescriptions(binding).setVertexAttributeDescriptions(attributes);
    constexpr vk::PipelineInputAssemblyStateCreateInfo assembly{{}, vk::PrimitiveTopology::eTriangleList};

    vk::PipelineViewportStateCreateInfo viewport;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;
    constexpr std::array dynamic_states{vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynamic;
    dynamic.setDynamicStates(dynamic_states);

    vk::PipelineRasterizationStateCreateInfo raster;
    raster.polygonMode = vk::PolygonMode::eFill;
    raster.cullMode = vk::CullModeFlagBits::eBack;
    raster.frontFace = vk::FrontFace::eCounterClockwise;
    raster.lineWidth = 1.0F;
    constexpr vk::PipelineMultisampleStateCreateInfo samples{{}, vk::SampleCountFlagBits::e1};

    vk::PipelineDepthStencilStateCreateInfo depth;
    depth.depthTestEnable = VK_TRUE;
    depth.depthWriteEnable = VK_TRUE;
    depth.depthCompareOp = vk::CompareOp::eLess;

    vk::PipelineColorBlendAttachmentState color;
    color.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                           vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    vk::PipelineColorBlendStateCreateInfo blend;
    blend.setAttachments(color);

    vk::PipelineRenderingCreateInfo rendering;
    rendering.setColorAttachmentFormats(color_format);
    rendering.depthAttachmentFormat = depth_format_;

    vk::GraphicsPipelineCreateInfo info;
    info.pNext = &rendering;
    info.setStages(stages);
    info.pVertexInputState = &vertex_input;
    info.pInputAssemblyState = &assembly;
    info.pViewportState = &viewport;
    info.pRasterizationState = &raster;
    info.pMultisampleState = &samples;
    info.pDepthStencilState = &depth;
    info.pColorBlendState = &blend;
    info.pDynamicState = &dynamic;
    info.layout = *layout_;
    pipeline_ = vk::raii::Pipeline{device_, nullptr, info};
    using occt_lab::render::NurbsGridVertex;
    binding.stride = sizeof(NurbsGridVertex);
    attributes[1].offset = offsetof(NurbsGridVertex, normal);
    attributes[1].format = vk::Format::eR32G32B32A32Sfloat;
    raster.cullMode = vk::CullModeFlagBits::eNone;
    nurbs_pipeline_ = vk::raii::Pipeline{device_, nullptr, info};
}

void BRepPreview::bind_mesh_pipeline(const vk::raii::CommandBuffer &commands, const vk::Extent2D extent,
                                     const ClipTransform &transform, const vk::Buffer vertices,
                                     const vk::Pipeline pipeline) const
{
    const auto width = static_cast<float>(extent.width);
    const auto height = static_cast<float>(extent.height);

    commands.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
    commands.setViewport(0, vk::Viewport{0.0F, 0.0F, width, height, 0.0F, 1.0F});
    commands.setScissor(0, vk::Rect2D{{0, 0}, extent});
    commands.pushConstants<ClipTransform>(*layout_, mesh_stages, 0, transform);
    commands.pushConstants<std::uint32_t>(*layout_, mesh_stages, sizeof(ClipTransform), 0U);
    commands.bindVertexBuffers(0, vertices, vk::DeviceSize{0});
}

void BRepPreview::draw_triangles(const vk::raii::CommandBuffer &commands, const vk::Extent2D extent,
                                 const ClipTransform &transform, const vk::Buffer vertices,
                                 const std::uint32_t vertex_count) const
{
    bind_mesh_pipeline(commands, extent, transform, vertices, *pipeline_);
    commands.draw(vertex_count, 1, 0, 0);
}

void BRepPreview::record(const vk::raii::CommandBuffer &commands, const vk::Extent2D extent, const PreviewShape shape,
                         const ClipTransform &transform) const
{
    const auto &mesh = mesh_for(shape);
    bind_mesh_pipeline(commands, extent, transform, *mesh.buffer, *pipeline_);
    commands.bindIndexBuffer(*mesh.buffer, mesh.index_offset, vk::IndexType::eUint32);
    const auto count = static_cast<std::uint32_t>(mesh.cpu_mesh.indices.size());
    if (!selected_face_)
    {
        commands.drawIndexed(count, 1, 0, 0, 0);
        return;
    }
    const auto &face = mesh.cpu_mesh.faces.at(selected_face_->face_index);
    // Each triangle is drawn once, with the normal depth test for all three ranges.
    if (face.first_index > 0)
    {
        commands.drawIndexed(face.first_index, 1, 0, 0, 0);
    }
    if (const auto after = face.first_index + face.index_count; after < count)
    {
        commands.drawIndexed(count - after, 1, after, 0, 0);
    }
    commands.pushConstants<std::uint32_t>(*layout_, mesh_stages, sizeof(ClipTransform), 1U);
    commands.drawIndexed(face.index_count, 1, face.first_index, 0, 0);
}

void BRepPreview::draw_indexed_triangles(const vk::raii::CommandBuffer &commands, const vk::Extent2D extent,
                                         const ClipTransform &transform, const vk::Buffer mesh,
                                         const vk::DeviceSize index_offset, const std::uint32_t index_count,
                                         const PreviewVertexLayout vertex_layout) const
{
    const auto pipeline = vertex_layout == PreviewVertexLayout::nurbs_grid ? *nurbs_pipeline_ : *pipeline_;
    bind_mesh_pipeline(commands, extent, transform, mesh, pipeline);
    commands.bindIndexBuffer(mesh, index_offset, vk::IndexType::eUint32);
    commands.drawIndexed(index_count, 1, 0, 0, 0);
}
