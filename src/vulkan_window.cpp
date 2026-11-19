#include "vulkan_window.hpp"

#include "render/brep_preview.hpp"
#include "render/curve_preview.hpp"
#include "render/orbit_camera.hpp"
#include "ui/box_editor.hpp"
#include "ui/curve_editor.hpp"

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
    const auto queues = device.getQueueFamilyProperties();
    std::optional<uint32_t> graphics_family;
    std::optional<uint32_t> present_family;
    for (uint32_t index = 0; index < queues.size(); ++index)
    {
        if (queues[index].queueCount == 0)
        {
            continue;
        }
        const bool graphics = static_cast<bool>(queues[index].queueFlags & vk::QueueFlagBits::eGraphics);
        const bool present = device.getSurfaceSupportKHR(index, surface);
        if (graphics && present)
        {
            return QueueFamilies{.graphics = index, .present = index};
        }
        if (graphics && !graphics_family)
        {
            graphics_family = index;
        }
        if (present && !present_family)
        {
            present_family = index;
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
};

PresentationDevice select_device(const vk::raii::Instance &instance, vk::SurfaceKHR surface)
{
    for (auto &device : instance.enumeratePhysicalDevices())
    {
        const auto properties = device.getProperties();
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

        const auto queue_flags = device.getQueueFamilyProperties()[families->graphics].queueFlags;
        std::println("Vulkan GPU: {} (API {})", properties.deviceName.data(),
                     format_api_version(properties.apiVersion));
        std::println("Vulkan queue families: graphics {} {}, present {}", families->graphics,
                     vk::to_string(queue_flags), families->present);
        std::println("Vulkan device extension: {}", VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        return {
            .physical_device = std::move(device),
            .families = *families,
            .properties = properties,
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

void configure_imgui_fonts(ImGuiIO &io)
{
    ImFontConfig font_config;
#ifdef _WIN32
    if (const auto *windows_dir = SDL_getenv("SystemRoot"); windows_dir && *windows_dir)
    {
        const auto path = std::format("{}/Fonts/segoeui.ttf", windows_dir);
        font_config.Flags |= ImFontFlags_NoLoadError;
        io.FontDefault = io.Fonts->AddFontFromFileTTF(path.c_str(), 0.0F, &font_config);
    }
#endif
    if (!io.FontDefault)
    {
        io.FontDefault = io.Fonts->AddFontDefaultVector(&font_config);
    }
    ImGui::GetStyle().FontSizeBase = 17.0F;
}

class ImGuiSession
{
public:
    explicit ImGuiSession(SDL_Window *window) : window_(window)
    {
        IMGUI_CHECKVERSION();
        context_.reset(ImGui::CreateContext());
        ImGui::SetCurrentContext(context_.get());
        auto &io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.IniFilename = nullptr;
        ImGui::StyleColorsDark();
        configure_imgui_fonts(io);
        if (!ImGui_ImplSDL3_InitForVulkan(window_))
        {
            throw_sdl_error("ImGui_ImplSDL3_InitForVulkan");
        }
    }

    ~ImGuiSession()
    {
        ImGui::SetCurrentContext(context_.get());
        shutdown_renderer();
        ImGui_ImplSDL3_Shutdown();
    }

    ImGuiSession(const ImGuiSession &) = delete;
    ImGuiSession &operator=(const ImGuiSession &) = delete;

    void initialize_renderer(ImGui_ImplVulkan_InitInfo &info) const
    {
        ImGui::SetCurrentContext(context_.get());
        shutdown_renderer();
        if (!ImGui_ImplVulkan_Init(&info))
        {
            throw std::runtime_error("ImGui Vulkan backend initialization failed");
        }
    }

    void begin_frame() const
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();

        const float display_scale = SDL_GetWindowDisplayScale(window_);
        const float pixel_density = SDL_GetWindowPixelDensity(window_);
        // ImGui already uses the framebuffer scale when baking glyphs.
        const float font_scale = display_scale > 0.0F && pixel_density > 0.0F ? display_scale / pixel_density : 1.0F;
        ImGui::GetStyle().FontScaleDpi = font_scale;
        ImGui::NewFrame();
    }

private:
    static void shutdown_renderer()
    {
        // The backend can register itself before creating all of its device objects.
        if (ImGui::GetIO().BackendRendererUserData)
        {
            ImGui_ImplVulkan_Shutdown();
        }
    }

    SDL_Window *window_;
    std::unique_ptr<ImGuiContext, decltype(&ImGui::DestroyContext)> context_{nullptr, ImGui::DestroyContext};
};

enum class PreviewMode
{
    box,
    cylinder,
    bspline,
};

void draw_status_panel(const PresentationDevice &selected, const Swapchain &swapchain, PreviewMode &mode,
                       ProjectionMode &projection, OrbitCamera &camera)
{
    ImGui::SetNextWindowPos({16.0F, 16.0F}, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Renderer status", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        const auto api_version = selected.properties.apiVersion;
        ImGui::TextUnformatted(selected.properties.deviceName.data());
        ImGui::Text("Device Vulkan %u.%u.%u", VK_API_VERSION_MAJOR(api_version), VK_API_VERSION_MINOR(api_version),
                    VK_API_VERSION_PATCH(api_version));
        ImGui::Text("Queue families   Graphics %u   Present %u", selected.families.graphics, selected.families.present);

        ImGui::SeparatorText("Swapchain");
        ImGui::Text("%u x %u px   %zu images", swapchain.extent.width, swapchain.extent.height,
                    swapchain.images.size());
        ImGui::Text("Frame rate   %.1f FPS", static_cast<double>(ImGui::GetIO().Framerate));

        ImGui::SeparatorText("Preview");
        const float combo_width = 9.0F * ImGui::GetFontSize();
        ImGui::SetNextItemWidth(combo_width);
        auto mode_index = static_cast<int>(mode);
        if (ImGui::Combo("Shape", &mode_index, "Box\0Cylinder\0B-spline\0"))
        {
            mode = static_cast<PreviewMode>(mode_index);
        }

        ImGui::SetNextItemWidth(combo_width);
        auto projection_index = static_cast<int>(projection);
        if (ImGui::Combo("Projection", &projection_index, "Perspective\0Orthographic\0"))
        {
            projection = static_cast<ProjectionMode>(projection_index);
        }

        if (ImGui::Button("Reset view"))
        {
            camera = OrbitCamera{};
        }
        ImGui::TextUnformatted("Middle-drag to orbit, wheel to zoom");
    }
    ImGui::End();
}

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

    bool rebuild_swapchain(const vk::Extent2D window_extent)
    {
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
        update_camera_input();
        draw_status_panel(selected_, swapchain, preview_mode_, projection_, camera_);
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
        case PreviewMode::bspline:
            curve_editor_.draw(curve_);
            break;
        }

        const double view_scale = solid ? preview_.view_scale(*solid) : 1.0;
        const auto transform =
            camera_.clip_transform(swapchain.extent.width, swapchain.extent.height, projection_, view_scale);
        if (preview_mode_ == PreviewMode::bspline)
        {
            draw_curve_preview(curve_.current(), transform, curve_editor_.selected_index());
        }
        ImGui::Render();

        const auto &image = swapchain.images[image_index];
        record_frame(image, swapchain, solid, transform, ImGui::GetDrawData());

        const vk::SemaphoreSubmitInfo acquire_wait{*image_available_, 0, color_output_stage};
        const vk::CommandBufferSubmitInfo commands{*command_buffer_, 1};
        const vk::SemaphoreSubmitInfo present_signal{*image.ready_to_present, 0, vk::PipelineStageFlagBits2::eAllCommands};
        vk::SubmitInfo2 submit;
        submit.setWaitSemaphoreInfos(acquire_wait)
            .setCommandBufferInfos(commands)
            .setSignalSemaphoreInfos(present_signal);
        device_.resetFences(*frame_finished_);
        graphics_queue_.submit2(submit, *frame_finished_);

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
    void update_camera_input()
    {
        const auto &io = ImGui::GetIO();
        if (constexpr auto orbit_button = ImGuiMouseButton_Middle; !ImGui::IsMouseDown(orbit_button))
        {
            orbiting_ = false;
        }
        else if (ImGui::IsMouseClicked(orbit_button))
        {
            orbiting_ = !io.WantCaptureMouse && io.KeyMods == ImGuiMod_None;
        }
        else if (orbiting_ && !io.WantCaptureMouse)
        {
            camera_.orbit(io.MouseDelta.x, io.MouseDelta.y);
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
                      const ClipTransform &transform, ImDrawData *draw_data) const
    {
        command_buffer_.reset();
        command_buffer_.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

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
    BoxEditor box_editor_{preview_.box_dimensions()};
    EditableCurve curve_;
    CurveEditor curve_editor_{curve_};
    PreviewMode preview_mode_ = PreviewMode::box;
    ProjectionMode projection_ = ProjectionMode::perspective;
    OrbitCamera camera_;
    bool orbiting_ = false;
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
