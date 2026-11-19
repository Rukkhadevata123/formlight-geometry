#include "brep_preview.hpp"

#include "geometry/brep_mesh.hpp"
#include "orbit_camera.hpp"

#include <BRepPrimAPI_MakeCylinder.hxx>
#include <SDL3/SDL.h>
#include <gp_Pnt.hxx>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <exception>
#include <format>
#include <memory>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
constexpr vk::ImageSubresourceRange depth_range{vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1};
constexpr auto depth_layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
constexpr double linear_deflection = 0.1;

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

uint32_t find_memory_type(const vk::PhysicalDeviceMemoryProperties &properties, const uint32_t type_bits,
                          const vk::MemoryPropertyFlags required)
{
    for (uint32_t index = 0; index < properties.memoryTypeCount; ++index)
    {
        if (const auto flags = properties.memoryTypes[index].propertyFlags;
            (type_bits & (1u << index)) != 0 && (flags & required) == required)
        {
            return index;
        }
    }
    throw std::runtime_error(std::format("No Vulkan memory type satisfies {}", vk::to_string(required)));
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

vk::raii::ShaderModule load_shader(const vk::raii::Device &device, const std::string_view filename)
{
    const auto *base_path = SDL_GetBasePath();
    if (!base_path)
    {
        throw std::runtime_error(std::format("SDL_GetBasePath: {}", SDL_GetError()));
    }

    const auto path = std::format("{}shaders/{}", base_path, filename);
    std::size_t byte_count = 0;
    const std::unique_ptr<void, decltype(&SDL_free)> bytes{SDL_LoadFile(path.c_str(), &byte_count), SDL_free};
    if (!bytes)
    {
        throw std::runtime_error(std::format("Could not load shader {}: {}", path, SDL_GetError()));
    }
    if (byte_count < 5 * sizeof(uint32_t) || byte_count % sizeof(uint32_t) != 0)
    {
        throw std::runtime_error(std::format("Invalid SPIR-V size for {}", path));
    }

    std::vector<uint32_t> words(byte_count / sizeof(uint32_t));
    std::memcpy(words.data(), bytes.get(), byte_count);
    if (words.front() != 0x07230203u)
    {
        throw std::runtime_error(std::format("Invalid SPIR-V header for {}", path));
    }
    vk::ShaderModuleCreateInfo info;
    info.setCode(words);
    return vk::raii::ShaderModule{device, info};
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
      cylinder_(upload_mesh(make_cylinder_mesh()))
{
    constexpr vk::PushConstantRange projection{vk::ShaderStageFlagBits::eVertex, 0, sizeof(ClipTransform)};
    vk::PipelineLayoutCreateInfo info;
    info.setPushConstantRanges(projection);
    layout_ = vk::raii::PipelineLayout{device_, info};
}

std::unique_ptr<BRepPreview::BoxPreview> BRepPreview::build_box(const BoxDimensions &dimensions) const
{
    auto model = make_box(dimensions);
    const auto [length, width, height] = model.dimensions;
    const gp_Pnt center{0.5 * length, 0.5 * width, 0.5 * height};
    const auto mesh = triangulate_brep(model.shape, center, linear_deflection);

    // Keep comparable boxes at the same scale and adjust framing for sizes outside that range.
    constexpr BoxDimensions defaults;
    const double default_diagonal = std::hypot(defaults.length, defaults.width, defaults.height);
    const double diagonal = std::hypot(length, width, height);
    const double view_diagonal = std::clamp(diagonal, 0.25 * default_diagonal, 1.125 * default_diagonal);
    return std::make_unique<BoxPreview>(BoxPreview{
        .model = std::move(model),
        .mesh = upload_mesh(mesh),
        .view_scale = diagonal / view_diagonal,
    });
}

BoxDimensions BRepPreview::box_dimensions() const
{
    return box_->model.dimensions;
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
    }
}

void BRepPreview::redo_box()
{
    if (redo_box_)
    {
        undo_box_ = std::exchange(box_, std::move(redo_box_));
    }
}

BRepPreview::MeshBuffer BRepPreview::upload_mesh(const BRepMeshData &mesh) const
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
        .index_count = static_cast<uint32_t>(mesh.indices.size()),
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

    constexpr vk::VertexInputBindingDescription binding{0, sizeof(MeshVertex), vk::VertexInputRate::eVertex};
    constexpr std::array attributes{
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
}

void BRepPreview::record(const vk::raii::CommandBuffer &commands, const vk::Extent2D extent, const PreviewShape shape,
                         const ClipTransform &transform) const
{
    const auto width = static_cast<float>(extent.width);
    const auto height = static_cast<float>(extent.height);
    const auto &mesh = shape == PreviewShape::box ? box_->mesh : cylinder_;

    commands.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline_);
    commands.setViewport(0, vk::Viewport{0.0F, 0.0F, width, height, 0.0F, 1.0F});
    commands.setScissor(0, vk::Rect2D{{0, 0}, extent});
    commands.pushConstants<ClipTransform>(*layout_, vk::ShaderStageFlagBits::eVertex, 0, transform);
    commands.bindVertexBuffers(0, *mesh.buffer, vk::DeviceSize{0});
    commands.bindIndexBuffer(*mesh.buffer, mesh.index_offset, vk::IndexType::eUint32);
    commands.drawIndexed(mesh.index_count, 1, 0, 0, 0);
}
