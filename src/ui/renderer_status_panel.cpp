#include "renderer_status_panel.hpp"

#include "render/preview/surface/surface_wireframe.hpp"
#include "render/viewport/orbit_camera.hpp"

#include <imgui.h>
#include <vulkan/vulkan_core.h>

namespace
{
const char *surface_type_name(const GeomAbs_SurfaceType type)
{
    switch (type)
    {
    case GeomAbs_Plane:
        return "Plane";
    case GeomAbs_Cylinder:
        return "Cylinder";
    case GeomAbs_Cone:
        return "Cone";
    case GeomAbs_Sphere:
        return "Sphere";
    case GeomAbs_Torus:
        return "Torus";
    case GeomAbs_BezierSurface:
        return "Bezier surface";
    case GeomAbs_BSplineSurface:
        return "B-spline surface";
    case GeomAbs_SurfaceOfRevolution:
        return "Surface of revolution";
    case GeomAbs_SurfaceOfExtrusion:
        return "Surface of extrusion";
    case GeomAbs_OffsetSurface:
        return "Offset surface";
    case GeomAbs_OtherSurface:
        return "Other surface";
    }
    return "Unknown surface";
}
} // namespace

void draw_status_panel(const RendererStatus &status, PreviewMode &mode, ProjectionMode &projection, OrbitCamera &camera,
                       const bool moving_pole, PatchLayers &patch_layers, bool &show_diagnostics,
                       PyramidControls &pyramid, const bool picking_points)
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
        auto mode_index = static_cast<int>(mode);
        constexpr const char *shapes[] = {"Box", "Cylinder", "Plate with hole (cut)", "Plate with hole (extrusion)",
                                          "NURBS curve", "NURBS surface", "Compute pyramid"};
        constexpr auto longest_label = shapes[static_cast<int>(PreviewMode::plate_with_hole_extrusion)];
        ImGui::SetNextItemWidth(ImGui::CalcTextSize(longest_label).x + ImGui::GetFrameHeight() +
                                2.0F * ImGui::GetStyle().FramePadding.x);
        const int shape_count = status.compute_supported ? IM_ARRAYSIZE(shapes) : IM_ARRAYSIZE(shapes) - 1;
        const bool shape_changed = ImGui::Combo("Shape", &mode_index, shapes, shape_count);
        if (shape_changed)
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
        if (mode == PreviewMode::box || mode == PreviewMode::cylinder || mode == PreviewMode::plate_with_hole_cut ||
            mode == PreviewMode::plate_with_hole_extrusion)
        {
            ImGui::TextUnformatted("Left-click a face to select it");
            if (status.selected_face && !shape_changed)
            {
                const auto &[face_index, surface_type, area_mm2] = *status.selected_face;
                ImGui::Text("Selected face: %zu", face_index + 1);
                ImGui::Text("Surface: %s", surface_type_name(surface_type));
                ImGui::Text("Area: %.8g mm\u00b2", area_mm2);
            }
        }
        else if (mode == PreviewMode::nurbs_curve)
        {
            ImGui::TextUnformatted(picking_points ? "Left-click to place points on XY, Esc to cancel"
                                                        : "Left-drag a blue pole to move it, Esc to cancel");
        }
        else if (mode == PreviewMode::nurbs_surface)
        {
            ImGui::Checkbox("Control net (blue)", &patch_layers.control_net);
            ImGui::Checkbox("Constant u (gold)", &patch_layers.constant_u);
            ImGui::Checkbox("Constant v (green)", &patch_layers.constant_v);
            if (picking_points)
            {
                ImGui::TextUnformatted("Left-click to place the next U/V pole on XY");
            }
            else if (patch_layers.control_net)
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
