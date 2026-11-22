#include "surface_sample_readout.hpp"

#include "geometry/surfaces/occt_bezier_patch.hpp"

#include <imgui.h>

using occt_lab::geometry::PatchError;
using occt_lab::geometry::SurfaceSample;

void draw_surface_sample(const SurfaceSample &sample)
{
    const auto &[position, normal] = sample;
    ImGui::Text("Position (mm)   (%.8g, %.8g, %.8g)", position.x, position.y, position.z);
    ImGui::Text("Unit normal     (%.8g, %.8g, %.8g)", normal.x, normal.y, normal.z);
}

void draw_bernstein_sample(const std::expected<SurfaceSample, PatchError> &sample)
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

void draw_surface_difference(const SurfaceSample &sample, const SurfaceSample &reference)
{
    const auto [distance_mm, angle_degrees] = compare_surface_samples(sample, reference);
    ImGui::Text("Position distance: %.3g mm", distance_mm);
    ImGui::Text("Normal angle: %.3g deg", angle_degrees);
}
