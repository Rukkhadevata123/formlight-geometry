#include "vulkan_window.hpp"

#include "render/compute/compute_buffer_check.hpp"
#include "render/compute/compute_pyramid.hpp"
#include "render/compute/nurbs_grid_mesh.hpp"
#include "render/preview/brep_preview.hpp"
#include "render/preview/surface/surface_wireframe.hpp"
#include "render/viewport/orbit_camera.hpp"
#include "ui/box_editor.hpp"
#include "ui/curve/curve_editor.hpp"
#include "ui/developer_diagnostics.hpp"
#include "ui/imgui_session.hpp"
#include "ui/renderer_status_panel.hpp"
#include "ui/surface/nurbs_surface_session.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>
#include <vulkan/vulkan_raii.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <format>
#include <limits>
#include <memory>
#include <optional>
#include <print>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
constexpr auto validation_layer = "VK_LAYER_KHRONOS_validation";
constexpr auto required_api_version = VK_API_VERSION_1_3;
constexpr auto color_output_stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
constexpr vk::ImageSubresourceRange color_range{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
constexpr std::array clear_color{0.025F, 0.055F, 0.095F, 1.0F};
constexpr auto acquire_timeout = std::chrono::nanoseconds{std::chrono::milliseconds{16}};
constexpr auto infinite_timeout = std::numeric_limits<uint64_t>::max();

[[noreturn]] void throw_sdl_error(std::string_view operation)
{
    throw std::runtime_error(std::format("{}: {}", operation, SDL_GetError()));
}

class SdlVideo
{
public:
    SdlVideo()
    {
        if (!SDL_Init(SDL_INIT_VIDEO))
        {
            throw_sdl_error("SDL_Init");
        }
    }

    ~SdlVideo()
    {
        SDL_Quit();
    }

    SdlVideo(const SdlVideo &) = delete;
    SdlVideo &operator=(const SdlVideo &) = delete;
};

std::string format_api_version(const uint32_t version)
{
    return std::format("{}.{}.{}", VK_API_VERSION_MAJOR(version), VK_API_VERSION_MINOR(version),
                       VK_API_VERSION_PATCH(version));
}

bool has_extension(const std::vector<vk::ExtensionProperties> &extensions, std::string_view name)
{
    return std::ranges::any_of(extensions,
                               [name](const auto &extension)
                               {
                                   return name == extension.extensionName.data();
                               });
}

VKAPI_ATTR vk::Bool32 VKAPI_CALL log_vulkan_message(const vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
                                                    vk::DebugUtilsMessageTypeFlagsEXT,
                                                    const vk::DebugUtilsMessengerCallbackDataEXT *data, void *)
{
    const auto priority =
        severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError ? SDL_LOG_PRIORITY_ERROR : SDL_LOG_PRIORITY_WARN;
    SDL_LogMessage(SDL_LOG_CATEGORY_RENDER, priority, "Vulkan: %s", data->pMessage);
    return VK_FALSE;
}

struct VulkanInstance
{
    vk::raii::Instance instance;
    vk::raii::DebugUtilsMessengerEXT debug_messenger;
};

VulkanInstance create_instance(const vk::raii::Context &context)
{
    Uint32 extension_count = 0;
    const auto *sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&extension_count);
    if (!sdl_extensions)
    {
        throw_sdl_error("SDL_Vulkan_GetInstanceExtensions");
    }
    std::vector extensions(sdl_extensions, sdl_extensions + extension_count);

    constexpr vk::ApplicationInfo application("occt-lab", 0, nullptr, 0, required_api_version);
    vk::InstanceCreateInfo instance_info;
    instance_info.pApplicationInfo = &application;

    vk::DebugUtilsMessengerCreateInfoEXT debug_info;
    debug_info.messageSeverity =
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
    debug_info.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                             vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                             vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
    debug_info.pfnUserCallback = log_vulkan_message;

    bool capture_debug_messages = false;
#ifndef NDEBUG
    const auto layers = context.enumerateInstanceLayerProperties();
    const bool enable_validation = std::ranges::any_of(
        layers, [](const auto &layer)
        {
            return std::string_view{layer.layerName.data()} == validation_layer;
        });
    if (enable_validation)
    {
        instance_info.setPEnabledLayerNames(validation_layer);
    }

    capture_debug_messages =
        has_extension(context.enumerateInstanceExtensionProperties(), VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    if (capture_debug_messages)
    {
        SDL_SetLogPriority(SDL_LOG_CATEGORY_RENDER, SDL_LOG_PRIORITY_WARN);
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        instance_info.pNext = &debug_info;
    }

    std::println("Vulkan validation layer: {}", enable_validation ? "enabled" : "unavailable");
#endif
    instance_info.setPEnabledExtensionNames(extensions);

    vk::raii::Instance instance(context, instance_info);
    vk::raii::DebugUtilsMessengerEXT messenger{nullptr};
    if (capture_debug_messages)
    {
        messenger = vk::raii::DebugUtilsMessengerEXT{instance, debug_info};
    }
    return {.instance = std::move(instance), .debug_messenger = std::move(messenger)};
}

vk::raii::SurfaceKHR create_surface(SDL_Window *window, const vk::raii::Instance &instance)
{
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, *instance, nullptr, &surface))
    {
        throw_sdl_error("SDL_Vulkan_CreateSurface");
    }
    return vk::raii::SurfaceKHR{instance, surface};
}

struct QueueFamilies
{
    uint32_t graphics;
    uint32_t present;
};

std::optional<QueueFamilies> find_queue_families(const vk::raii::PhysicalDevice &device, const vk::SurfaceKHR surface)
{
    const auto queues = device.getQueueFamilyProperties2();
    std::optional<uint32_t> graphics_family;
    std::optional<uint32_t> present_family;
    int best_score = -1;
    for (uint32_t index = 0; index < queues.size(); ++index)
    {
        const auto &queue = queues[index].queueFamilyProperties;
        if (queue.queueCount == 0)
        {
            continue;
        }
        const bool present = device.getSurfaceSupportKHR(index, surface);
        if (present && !present_family)
        {
            present_family = index;
        }
        if (queue.queueFlags & vk::QueueFlagBits::eGraphics)
        {
            // Prefer a graphics/compute queue, then presentation on the same family.
            const int score = (queue.queueFlags & vk::QueueFlagBits::eCompute ? 2 : 0) + static_cast<int>(present);
            if (score > best_score)
            {
                best_score = score;
                graphics_family = index;
                if (present)
                {
                    present_family = index;
                }
            }
        }
    }

    if (graphics_family && present_family)
    {
        return QueueFamilies{.graphics = *graphics_family, .present = *present_family};
    }
    return std::nullopt;
}

struct PresentationDevice
{
    vk::raii::PhysicalDevice physical_device;
    QueueFamilies families;
    vk::PhysicalDeviceProperties properties;
    bool compute_supported;
};

PresentationDevice select_device(const vk::raii::Instance &instance, vk::SurfaceKHR surface)
{
    for (auto &device : instance.enumeratePhysicalDevices())
    {
        const auto properties = device.getProperties2().properties;
        if (properties.apiVersion < required_api_version)
        {
            continue;
        }

        const auto features = device.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features>();
        if (const auto &vulkan13 = features.get<vk::PhysicalDeviceVulkan13Features>();
            !vulkan13.dynamicRendering || !vulkan13.synchronization2)
        {
            continue;
        }

        const auto families = find_queue_families(device, surface);
        if (!families || !has_extension(device.enumerateDeviceExtensionProperties(), VK_KHR_SWAPCHAIN_EXTENSION_NAME))
        {
            continue;
        }

        if (const auto capabilities = device.getSurfaceCapabilitiesKHR(surface);
            !(capabilities.supportedUsageFlags & vk::ImageUsageFlagBits::eColorAttachment) ||
            device.getSurfaceFormatsKHR(surface).empty())
        {
            continue;
        }

        const auto queue = device.getQueueFamilyProperties2()[families->graphics].queueFamilyProperties;
        std::println("Vulkan GPU: {} (API {})", properties.deviceName.data(),
                     format_api_version(properties.apiVersion));
        std::println("Vulkan queue families: graphics {} {}, present {}", families->graphics,
                     vk::to_string(queue.queueFlags), families->present);
        std::println("Vulkan device extension: {}", VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        const auto &limits = properties.limits;
        std::println("Compute limits: max local X {}, invocations {}, groups X {}, storage range {} bytes, "
                     "queue timestamp bits {}, period {} ns",
                     limits.maxComputeWorkGroupSize[0], limits.maxComputeWorkGroupInvocations,
                     limits.maxComputeWorkGroupCount[0], limits.maxStorageBufferRange, queue.timestampValidBits,
                     limits.timestampPeriod);
        return {
            .physical_device = std::move(device),
            .families = *families,
            .properties = properties,
            .compute_supported = static_cast<bool>(queue.queueFlags & vk::QueueFlagBits::eCompute),
        };
    }
    throw std::runtime_error(
        "No Vulkan 1.3 device supports dynamic rendering, synchronization2, and presentation to this window");
}

vk::raii::Device create_device(const PresentationDevice &selected)
{
    constexpr float priority = 1.0F;
    const auto [graphics, present] = selected.families;
    const std::array queues{
        vk::DeviceQueueCreateInfo{{}, graphics, 1, &priority},
        vk::DeviceQueueCreateInfo{{}, present, 1, &priority},
    };
    constexpr auto swapchain_extension = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

    vk::PhysicalDeviceVulkan13Features features;
    features.dynamicRendering = VK_TRUE;
    features.synchronization2 = VK_TRUE;

    vk::DeviceCreateInfo device_info;
    device_info.pNext = &features;
    device_info.pQueueCreateInfos = queues.data();
    device_info.queueCreateInfoCount = graphics == present ? 1 : 2;
    device_info.setPEnabledExtensionNames(swapchain_extension);
    return vk::raii::Device{selected.physical_device, device_info};
}

vk::Extent2D choose_extent(const vk::SurfaceCapabilitiesKHR &capabilities, const vk::Extent2D window_extent)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return capabilities.currentExtent;
    }
    return {
        std::clamp(window_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
        std::clamp(window_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height),
    };
}

vk::SurfaceFormatKHR choose_surface_format(const std::vector<vk::SurfaceFormatKHR> &formats)
{
    if (formats.empty())
    {
        throw std::runtime_error("Vulkan surface has no supported formats");
    }
    if (formats.size() == 1 && formats.front().format == vk::Format::eUndefined)
    {
        return {vk::Format::eB8G8R8A8Srgb, formats.front().colorSpace};
    }

    const auto preferred =
        std::ranges::find(formats, vk::SurfaceFormatKHR{vk::Format::eB8G8R8A8Srgb, vk::ColorSpaceKHR::eSrgbNonlinear});
    return preferred != formats.end() ? *preferred : formats.front();
}

vk::CompositeAlphaFlagBitsKHR choose_composite_alpha(const vk::CompositeAlphaFlagsKHR supported)
{
    using Alpha = vk::CompositeAlphaFlagBitsKHR;
    for (const auto mode : {Alpha::eOpaque, Alpha::ePreMultiplied, Alpha::ePostMultiplied, Alpha::eInherit})
    {
        if (supported & mode)
        {
            return mode;
        }
    }
    throw std::runtime_error("Vulkan surface has no supported composite alpha mode");
}

struct SwapchainImage
{
    vk::Image handle;
    vk::raii::ImageView view;
    vk::raii::Semaphore ready_to_present;
};

struct Swapchain
{
    vk::raii::SwapchainKHR handle;
    std::vector<SwapchainImage> images;
    PreviewDepthTarget depth;
    vk::Extent2D extent;
    vk::Format color_format;
    uint32_t min_image_count;
};

class Renderer
{
public:
  Renderer(SDL_Window *window, const vk::raii::Instance &instance, const vk::SurfaceKHR surface)
      : instance_(*instance), surface_(surface), selected_(select_device(instance, surface)),
        device_(create_device(selected_)), graphics_queue_(device_, selected_.families.graphics, 0),
        present_queue_(device_, selected_.families.present, 0),
        command_pool_(device_, {vk::CommandPoolCreateFlagBits::eResetCommandBuffer, selected_.families.graphics}),
        image_available_(device_, vk::SemaphoreCreateInfo{}),
        frame_finished_(device_, {vk::FenceCreateFlagBits::eSignaled}), preview_(selected_.physical_device, device_),
        imgui_(window)
  {
      const vk::CommandBufferAllocateInfo allocation{*command_pool_, vk::CommandBufferLevel::ePrimary, 1};
      vk::raii::CommandBuffers commands{device_, allocation};
      command_buffer_ = std::move(commands.front());
  }

    ~Renderer()
    {
        if (const auto result = device_.getDispatcher()->vkDeviceWaitIdle(static_cast<VkDevice>(*device_)); result != VK_SUCCESS)
        {
            SDL_LogError(SDL_LOG_CATEGORY_RENDER, "vkDeviceWaitIdle at shutdown: %d", static_cast<int>(result));
        }
    }

    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;

    void cancel_viewport_gesture()
    {
        curve_editor_.cancel_drag(curve_);
        if (nurbs_)
        {
            nurbs_->cancel_drag();
        }
        camera_drag_ = CameraDrag::none;
    }

    bool rebuild_swapchain(const vk::Extent2D window_extent)
    {
        cancel_viewport_gesture();
        const auto capabilities = selected_.physical_device.getSurfaceCapabilitiesKHR(surface_);
        const auto extent = choose_extent(capabilities, window_extent);
        if (extent.width == 0 || extent.height == 0)
        {
            return false;
        }

        const auto format = choose_surface_format(selected_.physical_device.getSurfaceFormatsKHR(surface_));
        const std::array queue_indices{selected_.families.graphics, selected_.families.present};

        vk::SwapchainCreateInfoKHR info;
        info.surface = surface_;
        info.minImageCount = capabilities.minImageCount;
        if (capabilities.maxImageCount == 0 || info.minImageCount < capabilities.maxImageCount)
        {
            ++info.minImageCount;
        }
        info.imageFormat = format.format;
        info.imageColorSpace = format.colorSpace;
        info.imageExtent = extent;
        info.imageArrayLayers = 1;
        info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
        info.imageSharingMode = vk::SharingMode::eExclusive;
        if (queue_indices[0] != queue_indices[1])
        {
            info.imageSharingMode = vk::SharingMode::eConcurrent;
            info.setQueueFamilyIndices(queue_indices);
        }
        info.preTransform = capabilities.currentTransform;
        info.compositeAlpha = choose_composite_alpha(capabilities.supportedCompositeAlpha);
        info.presentMode = vk::PresentModeKHR::eFifo;
        info.clipped = VK_TRUE;

        if (swapchain_)
        {
            device_.waitIdle();
            info.oldSwapchain = *swapchain_->handle;
        }

        Swapchain replacement{
            .handle = vk::raii::SwapchainKHR{device_, info},
            .images = {},
            .depth = preview_.create_depth_target(extent),
            .extent = extent,
            .color_format = format.format,
            .min_image_count = info.minImageCount,
        };
        const auto images = replacement.handle.getImages();
        replacement.images.reserve(images.size());
        for (const auto image : images)
        {
            vk::raii::ImageView view{device_, {{}, image, vk::ImageViewType::e2D, format.format, {}, color_range}};

            // A submit fence covers command buffers, but present semaphores need image reacquisition for reuse.
            replacement.images.push_back({
                .handle = image,
                .view = std::move(view),
                .ready_to_present = vk::raii::Semaphore{device_, vk::SemaphoreCreateInfo{}},
            });
        }

        const bool color_format_changed = !swapchain_ || replacement.color_format != swapchain_->color_format;
        if (color_format_changed)
        {
            preview_.create_pipeline(replacement.color_format);
        }
        if (color_format_changed ||
            replacement.min_image_count != swapchain_->min_image_count ||
            replacement.images.size() != swapchain_->images.size())
        {
            initialize_imgui_renderer(replacement);
        }

        swapchain_.emplace(std::move(replacement));
        return true;
    }

    [[nodiscard]] bool draw_frame()
    {
        if (const auto wait_result = device_.waitForFences(*frame_finished_, VK_TRUE, infinite_timeout); wait_result != vk::Result::eSuccess)
        {
            throw vk::SystemError(vk::make_error_code(wait_result), "waiting for the previous frame");
        }
        if (nurbs_readback_pending_)
        {
            try
            {
                nurbs_controls_.report = gpu_nurbs_->read_report();
            }
            catch (const vk::DeviceLostError &)
            {
                throw;
            }
            catch (const std::exception &error)
            {
                nurbs_controls_.report = error.what();
            }
            nurbs_readback_pending_ = false;
        }
        if (compute_readback_pending_)
        {
            compute_readback_pending_ = false;
            try
            {
                compute_status_ = compute_check_->read_result();
            }
            catch (const std::exception &error)
            {
                compute_status_ = error.what();
            }
        }

        const auto &swapchain = *swapchain_;
        // A timed-out acquire leaves the output index unwritten.
        uint32_t image_index = 0;
        const auto acquire_result = static_cast<vk::Result>(device_.getDispatcher()->vkAcquireNextImageKHR(
            static_cast<VkDevice>(*device_), static_cast<VkSwapchainKHR>(*swapchain.handle), acquire_timeout.count(),
            static_cast<VkSemaphore>(*image_available_), VK_NULL_HANDLE, &image_index));
        switch (acquire_result)
        {
        case vk::Result::eSuccess:
        case vk::Result::eSuboptimalKHR:
            break;
        case vk::Result::eErrorOutOfDateKHR:
            return false;
        case vk::Result::eTimeout:
        case vk::Result::eNotReady:
            return true;
        default:
            throw vk::SystemError(vk::make_error_code(acquire_result), "acquiring a swapchain image");
        }

        imgui_.begin_frame();
        const bool moving_pole = curve_editor_.tracking_pole() || (nurbs_ && nurbs_->tracking_pole());
        if (!moving_pole)
        {
            update_camera_input(swapchain.extent);
        }
        const RendererStatus status{
            .device_name = selected_.properties.deviceName.data(),
            .api_version = selected_.properties.apiVersion,
            .graphics_family = selected_.families.graphics,
            .present_family = selected_.families.present,
            .compute_supported = selected_.compute_supported,
            .width = swapchain.extent.width,
            .height = swapchain.extent.height,
            .image_count = swapchain.images.size(),
            .selected_face = preview_.selected_face(),
        };
        const auto previous_mode = preview_mode_;
        draw_status_panel(status, preview_mode_, projection_, camera_, moving_pole, patch_layers_, show_diagnostics_,
                          pyramid_controls_, curve_editor_.picking_points() || (nurbs_ && nurbs_->picking_net()));
        if (preview_mode_ != previous_mode)
        {
            preview_.clear_selection();
        }
        if (preview_mode_ != PreviewMode::nurbs_curve)
        {
            curve_editor_.cancel_point_picking();
        }
        if (nurbs_ && preview_mode_ != PreviewMode::nurbs_surface)
        {
            nurbs_->cancel_creation();
        }
        const bool run_compute =
            draw_developer_diagnostics(show_diagnostics_, selected_.compute_supported, compute_status_);
        if (run_compute && !compute_check_)
        {
            try
            {
                compute_check_.emplace(selected_.physical_device, device_);
            }
            catch (const std::exception &error)
            {
                compute_status_ = error.what();
            }
        }
        std::optional<PreviewShape> solid;
        switch (preview_mode_)
        {
        case PreviewMode::box:
            // The last submission has finished, so the editor can replace or restore a mesh.
            box_editor_.draw(preview_);
            solid = PreviewShape::box;
            break;
        case PreviewMode::cylinder:
            solid = PreviewShape::cylinder;
            break;
        case PreviewMode::drilled_plate:
            solid = PreviewShape::drilled_plate;
            break;
        case PreviewMode::compute_pyramid:
            if (!pyramid_ && pyramid_controls_.error.empty())
            {
                try
                {
                    pyramid_.emplace(selected_.physical_device, device_);
                }
                catch (const vk::DeviceLostError &)
                {
                    throw;
                }
                catch (const std::exception &error)
                {
                    pyramid_controls_.error = error.what();
                }
            }
            break;
        case PreviewMode::nurbs_curve:
        case PreviewMode::nurbs_surface:
            break;
        }

        const double view_scale = solid ? preview_.view_scale(*solid) : 1.0;
        const auto transform =
            camera_.clip_transform(swapchain.extent.width, swapchain.extent.height, projection_, view_scale);
        const auto &io = ImGui::GetIO();
        const bool pole_click = ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.WantCaptureMouse &&
                                io.KeyMods == ImGuiMod_None && !ImGui::IsMouseDown(ImGuiMouseButton_Middle);
        if (solid && pole_click)
        {
            const auto &viewport = *ImGui::GetMainViewport();
            const auto mouse = ImGui::GetMousePos();
            const double x = 2.0 * (mouse.x - viewport.Pos.x) / viewport.Size.x - 1.0;
            const double y = 2.0 * (mouse.y - viewport.Pos.y) / viewport.Size.y - 1.0;
            preview_.pick_face(*solid, transform, x, y);
        }
        else if (preview_mode_ == PreviewMode::nurbs_curve)
        {
            curve_editor_.handle_viewport_input(curve_, transform, pole_click);
            curve_editor_.draw(curve_);
            curve_editor_.draw_preview(curve_, transform);
        }
        else if (preview_mode_ == PreviewMode::nurbs_surface)
        {
            if (!nurbs_)
            {
                nurbs_.emplace();
            }
            const auto &snapshot = nurbs_->draw(transform, patch_layers_, pole_click,
                                                selected_.compute_supported ? &nurbs_controls_ : nullptr);
            if (selected_.compute_supported && !nurbs_->picking_net() &&
                (nurbs_controls_.visible || nurbs_controls_.requested))
            {
                try
                {
                    if (!gpu_nurbs_)
                    {
                        gpu_nurbs_.emplace(selected_.physical_device, device_, selected_.properties.limits.maxStorageBufferRange);
                    }
                    gpu_nurbs_->prepare(snapshot, nurbs_controls_.grid);
                }
                catch (const vk::DeviceLostError &)
                {
                    throw;
                }
                catch (const std::exception &error)
                {
                    gpu_nurbs_.reset();
                    nurbs_controls_.visible = false;
                    nurbs_controls_.report = error.what();
                }
            }
        }
        ImGui::Render();

        const auto &image = swapchain.images[image_index];
        const bool dispatch_compute = run_compute && compute_check_.has_value();
        record_frame(image, swapchain, solid, transform, ImGui::GetDrawData(), dispatch_compute);

        const vk::SemaphoreSubmitInfo acquire_wait{*image_available_, 0, color_output_stage};
        const vk::CommandBufferSubmitInfo commands{*command_buffer_, 1};
        const vk::SemaphoreSubmitInfo present_signal{*image.ready_to_present, 0, vk::PipelineStageFlagBits2::eAllCommands};
        vk::SubmitInfo2 submit;
        submit.setWaitSemaphoreInfos(acquire_wait)
            .setCommandBufferInfos(commands)
            .setSignalSemaphoreInfos(present_signal);
        device_.resetFences(*frame_finished_);
        graphics_queue_.submit2(submit, *frame_finished_);
        compute_readback_pending_ = dispatch_compute;
        nurbs_readback_pending_ = preview_mode_ == PreviewMode::nurbs_surface && gpu_nurbs_ && nurbs_controls_.requested;

        vk::PresentInfoKHR present;
        present.setWaitSemaphores(*image.ready_to_present)
               .setSwapchains(*swapchain.handle)
               .setImageIndices(image_index);
        try
        {
            const auto result = present_queue_.presentKHR(present);
            return result == vk::Result::eSuccess && acquire_result == vk::Result::eSuccess;
        }
        catch (const vk::OutOfDateKHRError &)
        {
            return false;
        }
    }

private:
    enum class CameraDrag
    {
        none,
        orbit,
        pan,
    };

    void update_camera_input(const vk::Extent2D extent)
    {
        const auto &io = ImGui::GetIO();
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Middle))
        {
            camera_drag_ = CameraDrag::none;
        }
        else if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
        {
            camera_drag_ = CameraDrag::none;
            if (!io.WantCaptureMouse && io.KeyMods == ImGuiMod_Shift)
            {
                camera_drag_ = CameraDrag::pan;
            }
            else if (!io.WantCaptureMouse && io.KeyMods == ImGuiMod_None)
            {
                camera_drag_ = CameraDrag::orbit;
            }
        }
        else if (camera_drag_ == CameraDrag::orbit && !io.WantCaptureMouse)
        {
            camera_.orbit(io.MouseDelta.x, io.MouseDelta.y);
        }
        else if (camera_drag_ == CameraDrag::pan && !io.WantCaptureMouse)
        {
            if (const auto size = ImGui::GetMainViewport()->Size; size.x > 0.0F && size.y > 0.0F)
            {
                const double short_side = std::min(extent.width, extent.height);
                camera_.pan(io.MouseDelta.x / size.x * (extent.width / short_side),
                            io.MouseDelta.y / size.y * (extent.height / short_side));
            }
        }

        if (!io.WantCaptureMouse && io.MouseWheel != 0.0F)
        {
            camera_.zoom(io.MouseWheel);
        }
    }

    void initialize_imgui_renderer(const Swapchain &swapchain) const
    {
        constexpr uint32_t backend_min_image_count = 2;
        const auto min_image_count = std::max(backend_min_image_count, swapchain.min_image_count);
        if (swapchain.images.size() < min_image_count)
        {
            throw std::runtime_error("ImGui requires at least two swapchain images");
        }

        ImGui_ImplVulkan_InitInfo info{};
        info.ApiVersion = required_api_version;
        info.Instance = static_cast<VkInstance>(instance_);
        info.PhysicalDevice = static_cast<VkPhysicalDevice>(*selected_.physical_device);
        info.Device = static_cast<VkDevice>(*device_);
        info.QueueFamily = selected_.families.graphics;
        info.Queue = static_cast<VkQueue>(*graphics_queue_);
        info.DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE;
        info.MinImageCount = min_image_count;
        info.ImageCount = static_cast<uint32_t>(swapchain.images.size());
        info.UseDynamicRendering = true;

        const auto color_format = static_cast<VkFormat>(swapchain.color_format);
        auto &pipeline = info.PipelineInfoMain.PipelineRenderingCreateInfo;
        pipeline.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        pipeline.colorAttachmentCount = 1;
        pipeline.pColorAttachmentFormats = &color_format;
        pipeline.depthAttachmentFormat = static_cast<VkFormat>(preview_.depth_format());
        info.CheckVkResultFn = [](const VkResult result)
        {
            if (result < VK_SUCCESS)
            {
                throw vk::SystemError(vk::make_error_code(static_cast<vk::Result>(result)), "ImGui Vulkan backend");
            }
        };

        imgui_.initialize_renderer(info);
    }

    void record_frame(const SwapchainImage &image, const Swapchain &swapchain, const std::optional<PreviewShape> solid,
                      const ClipTransform &transform, ImDrawData *draw_data, const bool dispatch_compute)
    {
        command_buffer_.reset();
        command_buffer_.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
        const bool nurbs_ready =
            preview_mode_ == PreviewMode::nurbs_surface && gpu_nurbs_.has_value() && !nurbs_->picking_net();
        if (nurbs_ready && (nurbs_controls_.visible || nurbs_controls_.requested))
        {
            gpu_nurbs_->dispatch_if_needed(command_buffer_);
        }
        if (dispatch_compute)
        {
            compute_check_->record(command_buffer_);
        }
        const bool draw_pyramid = preview_mode_ == PreviewMode::compute_pyramid && pyramid_.has_value();
        if (draw_pyramid)
        {
            pyramid_->generate_if_changed(command_buffer_, static_cast<uint32_t>(pyramid_controls_.height_mm));
        }

        // Match the acquire semaphore's wait stage before discarding the previous image contents.
        vk::ImageMemoryBarrier2 to_color;
        to_color.srcStageMask = color_output_stage;
        to_color.dstStageMask = color_output_stage;
        to_color.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite;
        to_color.oldLayout = vk::ImageLayout::eUndefined;
        to_color.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
        to_color.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        to_color.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        to_color.image = image.handle;
        to_color.subresourceRange = color_range;

        const std::array barriers{to_color, swapchain.depth.render_barrier()};
        vk::DependencyInfo dependency;
        command_buffer_.pipelineBarrier2(dependency.setImageMemoryBarriers(barriers));

        vk::RenderingAttachmentInfo color_attachment;
        color_attachment.imageView = *image.view;
        color_attachment.imageLayout = to_color.newLayout;
        color_attachment.loadOp = vk::AttachmentLoadOp::eClear;
        color_attachment.storeOp = vk::AttachmentStoreOp::eStore;
        color_attachment.clearValue = vk::ClearValue{vk::ClearColorValue{clear_color}};

        const auto depth_attachment = swapchain.depth.attachment();
        vk::RenderingInfo rendering;
        rendering.renderArea = vk::Rect2D{{0, 0}, swapchain.extent};
        rendering.layerCount = 1;
        rendering.setColorAttachments(color_attachment);
        rendering.pDepthAttachment = &depth_attachment;
        command_buffer_.beginRendering(rendering);
        if (solid)
        {
            preview_.record(command_buffer_, swapchain.extent, *solid, transform);
        }
        else if (draw_pyramid)
        {
            preview_.draw_triangles(command_buffer_, swapchain.extent, transform, pyramid_->vertices(),
                                    ComputePyramid::vertex_count);
        }
        else if (nurbs_ready && nurbs_controls_.visible)
        {
            gpu_nurbs_->draw(command_buffer_, preview_, swapchain.extent, transform);
        }
        ImGui_ImplVulkan_RenderDrawData(draw_data, *command_buffer_);
        command_buffer_.endRendering();

        // The present semaphore makes the completed transition visible to the presentation engine.
        auto to_present = to_color;
        to_present.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
        to_present.dstStageMask = vk::PipelineStageFlagBits2::eNone;
        to_present.dstAccessMask = vk::AccessFlagBits2::eNone;
        to_present.oldLayout = color_attachment.imageLayout;
        to_present.newLayout = vk::ImageLayout::ePresentSrcKHR;
        command_buffer_.pipelineBarrier2(dependency.setImageMemoryBarriers(to_present));

        command_buffer_.end();
    }

    vk::Instance instance_;
    vk::SurfaceKHR surface_;
    PresentationDevice selected_;
    vk::raii::Device device_;
    vk::raii::Queue graphics_queue_;
    vk::raii::Queue present_queue_;

    vk::raii::CommandPool command_pool_;
    vk::raii::CommandBuffer command_buffer_{nullptr};
    vk::raii::Semaphore image_available_;
    vk::raii::Fence frame_finished_;
    BRepPreview preview_;
    std::optional<ComputeBufferCheck> compute_check_;
    bool show_diagnostics_ = false;
    std::string compute_status_;
    bool compute_readback_pending_ = false;
    std::optional<ComputePyramid> pyramid_;
    PyramidControls pyramid_controls_;
    BoxEditor box_editor_{preview_.box_dimensions()};
    EditableCurve curve_;
    CurveEditor curve_editor_{curve_};
    PatchLayers patch_layers_;
    std::optional<NurbsSurfaceSession> nurbs_;
    NurbsMeshControls nurbs_controls_;
    std::optional<NurbsGridMesh> gpu_nurbs_;
    bool nurbs_readback_pending_ = false;
    PreviewMode preview_mode_ = PreviewMode::box;
    ProjectionMode projection_ = ProjectionMode::perspective;
    OrbitCamera camera_;
    CameraDrag camera_drag_ = CameraDrag::none;
    std::optional<Swapchain> swapchain_;
    ImGuiSession imgui_;
};

void run_event_loop(SDL_Window *window, Renderer &renderer)
{
    bool rebuild_swapchain = true;
    while (true)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
            switch (event.type)
            {
            case SDL_EVENT_QUIT:
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                return;
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            case SDL_EVENT_WINDOW_RESTORED:
                rebuild_swapchain = true;
                break;
            case SDL_EVENT_WINDOW_FOCUS_LOST:
            case SDL_EVENT_WINDOW_MINIMIZED:
                renderer.cancel_viewport_gesture();
                break;
            default:
                break;
            }
        }

        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)
        {
            SDL_WaitEventTimeout(nullptr, 50);
            continue;
        }

        if (rebuild_swapchain)
        {
            int width = 0;
            int height = 0;
            if (!SDL_GetWindowSizeInPixels(window, &width, &height))
            {
                throw_sdl_error("SDL_GetWindowSizeInPixels");
            }
            if (width <= 0 || height <= 0 ||
                !renderer.rebuild_swapchain({static_cast<uint32_t>(width), static_cast<uint32_t>(height)}))
            {
                SDL_WaitEventTimeout(nullptr, 50);
                continue;
            }
        }

        rebuild_swapchain = !renderer.draw_frame();
    }
}
} // namespace

void run_vulkan_window()
{
    const SdlVideo video;
    constexpr auto window_flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    const std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window{
        SDL_CreateWindow("occt-lab", 960, 640, window_flags),
        SDL_DestroyWindow,
    };
    if (!window)
    {
        throw_sdl_error("SDL_CreateWindow");
    }

    const auto get_instance_proc = SDL_Vulkan_GetVkGetInstanceProcAddr();
    if (!get_instance_proc)
    {
        throw_sdl_error("SDL_Vulkan_GetVkGetInstanceProcAddr");
    }
    const vk::raii::Context context{reinterpret_cast<PFN_vkGetInstanceProcAddr>(get_instance_proc)};
    const auto loader_version =
        context.getDispatcher()->vkEnumerateInstanceVersion ? context.enumerateInstanceVersion() : VK_API_VERSION_1_0;
    std::println("Vulkan loader API: {}", format_api_version(loader_version));
    if (loader_version < required_api_version)
    {
        throw std::runtime_error("Vulkan loader 1.3 or newer is required");
    }

    const auto [instance, debug_messenger] = create_instance(context);
    const auto surface = create_surface(window.get(), instance);
    Renderer renderer{window.get(), instance, *surface};
    std::println("Vulkan 1.3 features: dynamicRendering, synchronization2");
    std::fflush(stdout);
    run_event_loop(window.get(), renderer);
}
