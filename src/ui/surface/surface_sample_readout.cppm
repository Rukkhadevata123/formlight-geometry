module;

#include <imgui.h>

export module formlight_geometry.ui.surface.surface_sample_readout;

import std;
import formlight_geometry.geometry.surfaces.bezier_patch;
import formlight_geometry.geometry.surfaces.occt_bezier_patch;

export inline constexpr char parameter_slider_hint[] = "Ctrl+click a slider to type a value, then press Enter.";

using formlight_geometry::geometry::PatchError;
using formlight_geometry::geometry::SurfaceSample;

export void draw_surface_sample(const SurfaceSample &sample)
{
    const auto &[position, normal] = sample;
    ImGui::Text("Position (mm)   (%.8g, %.8g, %.8g)", position.x, position.y, position.z);
    ImGui::Text("Unit normal     (%.8g, %.8g, %.8g)", normal.x, normal.y, normal.z);
}

export void draw_bernstein_sample(const std::expected<SurfaceSample, PatchError> &sample)
{
    if (!sample && sample.error() == PatchError::NotPolynomialBicubic)
    {
        ImGui::TextWrapped("Bernstein comparison requires a single bicubic span with equal weights.");
        return;
    }
    ImGui::SeparatorText("Bernstein evaluation");
    if (sample)
    {
        draw_surface_sample(*sample);
    }
    else if (sample.error() == PatchError::DegenerateNormal)
    {
        ImGui::TextUnformatted("The Bernstein normal is undefined at this parameter.");
    }
    else if (sample.error() == PatchError::NonFiniteEvaluation)
    {
        ImGui::TextUnformatted("Bernstein evaluation exceeded the finite coordinate range.");
    }
    else
    {
        ImGui::TextUnformatted("Bernstein evaluation requires a finite control net and parameters inside the domain.");
    }
}

export void draw_surface_difference(const SurfaceSample &sample, const SurfaceSample &reference)
{
    const auto [distance_mm, angle_degrees] = compare_surface_samples(sample, reference);
    ImGui::Text("Position distance: %.3g mm", distance_mm);
    ImGui::Text("Normal angle: %.3g deg", angle_degrees);
}
