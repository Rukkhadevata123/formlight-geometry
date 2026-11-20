#include "bezier_patch_inspector.hpp"

#include "render/bezier_patch_preview.hpp"

#include <imgui.h>

BezierPatchInspector::BezierPatchInspector(BezierPatchPreview &preview)
    : preview_(preview), sample_(preview_.patch().evaluate(u_, v_))
{
}

void BezierPatchInspector::draw()
{
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
            sample_ = preview_.patch().evaluate(u_, v_);
        }

        if (sample_)
        {
            const auto &[position, normal] = *sample_;
            ImGui::Text("Position (mm)   (%.5f, %.5f, %.5f)", position.x, position.y, position.z);
            ImGui::Text("Unit normal     (%.5f, %.5f, %.5f)", normal.x, normal.y, normal.z);
        }
        else if (sample_.error() == occt_lab::geometry::PatchError::DegenerateNormal)
        {
            ImGui::TextUnformatted("The surface normal is undefined at this parameter.");
        }
        else
        {
            ImGui::TextUnformatted("Choose finite u and v values between 0 and 1.");
        }
        draw_sampling_controls();
    }
    ImGui::End();
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
