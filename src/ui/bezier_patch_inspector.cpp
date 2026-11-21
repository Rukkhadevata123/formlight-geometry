#include "bezier_patch_inspector.hpp"

#include "render/bezier_patch_preview.hpp"

#include <imgui.h>

namespace
{
void draw_mesh_controls(PatchMeshControls &mesh)
{
    ImGui::SeparatorText("Vulkan surface mesh");
    ImGui::Checkbox("Show GPU mesh", &mesh.visible);
    if (mesh.visible)
    {
        constexpr uint32_t minimum = 1;
        constexpr uint32_t maximum = occt_lab::geometry::PatchGrid::max_segments;
        const auto previous = mesh.grid;
        ImGui::SliderScalar("U subdivisions", ImGuiDataType_U32, &mesh.grid.u_segments, &minimum, &maximum, "%u",
                            ImGuiSliderFlags_AlwaysClamp);
        ImGui::SliderScalar("V subdivisions", ImGuiDataType_U32, &mesh.grid.v_segments, &minimum, &maximum, "%u",
                            ImGuiSliderFlags_AlwaysClamp);
        if (previous != mesh.grid)
        {
            mesh.report.clear();
        }
        mesh.compare_requested = ImGui::Button("Compare grid with OCCT");
    }
    if (!mesh.report.empty())
    {
        ImGui::TextUnformatted(mesh.report.c_str());
    }
}

void draw_surface_sample(const occt_lab::geometry::SurfaceSample &sample)
{
    const auto &[position, normal] = sample;
    ImGui::Text("Position (mm)   (%.5f, %.5f, %.5f)", position.x, position.y, position.z);
    ImGui::Text("Unit normal     (%.5f, %.5f, %.5f)", normal.x, normal.y, normal.z);
}
} // namespace

BezierPatchInspector::BezierPatchInspector(BezierPatchPreview &preview)
    : preview_(preview), reference_patch_(preview.patch())
{
    update_samples();
}

void BezierPatchInspector::update_samples()
{
    sample_ = preview_.patch().evaluate(u_, v_);
    reference_sample_ = reference_patch_.evaluate(u_, v_);
}

void BezierPatchInspector::draw(PatchMeshControls &mesh, const bool compute_supported)
{
    mesh.compare_requested = false;
    const auto *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({viewport->WorkPos.x + viewport->WorkSize.x - 16.0F, viewport->WorkPos.y + 16.0F},
                            ImGuiCond_FirstUseEver, {1.0F, 0.0F});
    if (ImGui::Begin("Bezier surface sample", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        constexpr double min_parameter = 0.0;
        constexpr double max_parameter = 1.0;
        ImGui::PushItemWidth(11.0F * ImGui::GetFontSize());
        const bool u_changed = ImGui::SliderScalar("u", ImGuiDataType_Double, &u_, &min_parameter, &max_parameter,
                                                   "%.4f", ImGuiSliderFlags_AlwaysClamp);
        const bool v_changed = ImGui::SliderScalar("v", ImGuiDataType_Double, &v_, &min_parameter, &max_parameter,
                                                   "%.4f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::PopItemWidth();
        if (u_changed || v_changed)
        {
            update_samples();
        }

        ImGui::SeparatorText("Bernstein evaluation");
        if (sample_)
        {
            draw_surface_sample(*sample_);
        }
        else if (sample_.error() == occt_lab::geometry::PatchError::DegenerateNormal)
        {
            ImGui::TextUnformatted("The surface normal is undefined at this parameter.");
        }
        else if (sample_.error() == occt_lab::geometry::PatchError::NonFiniteEvaluation)
        {
            ImGui::TextUnformatted("Bernstein evaluation exceeded the finite coordinate range.");
        }
        else
        {
            ImGui::TextUnformatted("Choose finite u and v values between 0 and 1.");
        }
        draw_reference();
        draw_sampling_controls();
        if (compute_supported)
        {
            draw_mesh_controls(mesh);
        }
    }
    ImGui::End();
}

void BezierPatchInspector::draw_reference() const
{
    ImGui::SeparatorText("OCCT reference");
    if (!reference_sample_)
    {
        ImGui::TextUnformatted(reference_sample_.error().c_str());
        return;
    }
    draw_surface_sample(*reference_sample_);
    if (sample_)
    {
        const auto [distance_mm, angle_degrees] = compare_surface_samples(*sample_, *reference_sample_);
        ImGui::Text("Position distance: %.3g mm", distance_mm);
        ImGui::Text("Normal angle: %.3g deg", angle_degrees);
    }
}

void BezierPatchInspector::draw_sampling_controls()
{
    using occt_lab::geometry::PatchSampling;
    auto sampling = preview_.sampling();
    ImGui::SeparatorText("Isoline sampling");
    ImGui::PushItemWidth(11.0F * ImGui::GetFontSize());
    ImGui::SliderInt("Constant u lines", &sampling.constant_u_lines, PatchSampling::minimum_count,
                     PatchSampling::max_lines, "%d", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SliderInt("Constant v lines", &sampling.constant_v_lines, PatchSampling::minimum_count,
                     PatchSampling::max_lines, "%d", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SliderInt("Points per line", &sampling.samples_per_line, PatchSampling::minimum_count,
                     PatchSampling::max_samples, "%d", ImGuiSliderFlags_AlwaysClamp);
    ImGui::PopItemWidth();
    if (sampling != preview_.sampling())
    {
        sampling_failed_ = !preview_.resample(sampling);
    }
    if (sampling_failed_)
    {
        ImGui::TextUnformatted("Could not sample the isolines. The previous settings are still in use.");
    }
}
