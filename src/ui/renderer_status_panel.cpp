#include "renderer_status_panel.hpp"

#include "render/preview/bezier_patch_preview.hpp"
#include "render/viewport/orbit_camera.hpp"

#include <imgui.h>
#include <vulkan/vulkan_core.h>

void draw_status_panel(const RendererStatus &status, PreviewMode &mode, ProjectionMode &projection, OrbitCamera &camera,
                       const bool moving_pole, PatchLayers &patch_layers, bool &show_diagnostics,
                       PyramidControls &pyramid)
{
    ImGui::SetNextWindowPos({16.0F, 16.0F}, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Renderer status", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_MenuBar))
    {
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("Tools"))
            {
                ImGui::MenuItem("Developer diagnostics", nullptr, &show_diagnostics);
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
        const auto api_version = status.api_version;
        ImGui::TextUnformatted(status.device_name);
        ImGui::Text("Device Vulkan %u.%u.%u", VK_API_VERSION_MAJOR(api_version), VK_API_VERSION_MINOR(api_version),
                    VK_API_VERSION_PATCH(api_version));
        ImGui::Text("Queue families   Graphics %u   Present %u", status.graphics_family, status.present_family);

        ImGui::SeparatorText("Swapchain");
        ImGui::Text("%u x %u px   %zu images", status.width, status.height, status.image_count);
        ImGui::Text("Frame rate   %.1f FPS", static_cast<double>(ImGui::GetIO().Framerate));

        ImGui::SeparatorText("Preview");
        ImGui::BeginDisabled(moving_pole);
        const float combo_width = 9.0F * ImGui::GetFontSize();
        ImGui::SetNextItemWidth(combo_width);
        auto mode_index = static_cast<int>(mode);
        constexpr const char *shapes[] = {"Box", "Cylinder", "B-spline", "Bezier patch", "Compute pyramid"};
        const int shape_count = status.compute_supported ? IM_ARRAYSIZE(shapes) : IM_ARRAYSIZE(shapes) - 1;
        if (ImGui::Combo("Shape", &mode_index, shapes, shape_count))
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
        ImGui::EndDisabled();
        ImGui::TextUnformatted("Middle-drag to orbit, Shift + middle-drag to pan");
        ImGui::TextUnformatted("Mouse wheel to zoom");
        if (mode == PreviewMode::bspline)
        {
            ImGui::TextUnformatted("Left-drag a blue pole to move it, Esc to cancel");
        }
        else if (mode == PreviewMode::bezier_patch)
        {
            ImGui::Checkbox("Control net (blue)", &patch_layers.control_net);
            ImGui::Checkbox("Constant u (gold)", &patch_layers.constant_u);
            ImGui::Checkbox("Constant v (green)", &patch_layers.constant_v);
            if (patch_layers.control_net)
            {
                ImGui::TextUnformatted("Left-drag a blue pole to move it, Esc to cancel");
            }
        }
        else if (mode == PreviewMode::compute_pyramid)
        {
            ImGui::SliderInt("Height (mm)", &pyramid.height_mm, 10, 90, "%d", ImGuiSliderFlags_AlwaysClamp);
            if (!pyramid.error.empty())
            {
                ImGui::TextWrapped("%s", pyramid.error.c_str());
                if (ImGui::Button("Retry pyramid"))
                {
                    pyramid.error.clear();
                }
            }
        }
    }
    ImGui::End();
}
