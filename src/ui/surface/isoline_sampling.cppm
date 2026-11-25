module;

#include <imgui.h>

export module formlight_geometry.ui.surface.isoline_sampling;

import formlight_geometry.geometry.surfaces.surface_isolines;

export [[nodiscard]] bool draw_isoline_sampling_controls(formlight_geometry::geometry::PatchSampling &sampling)
{
    using formlight_geometry::geometry::PatchSampling;
    const auto before = sampling;
    ImGui::SeparatorText("Isoline sampling");
    ImGui::PushItemWidth(11.0F * ImGui::GetFontSize());
    ImGui::SliderInt("Constant u lines", &sampling.constant_u_lines, PatchSampling::minimum_count,
                     PatchSampling::max_lines, "%d", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SliderInt("Constant v lines", &sampling.constant_v_lines, PatchSampling::minimum_count,
                     PatchSampling::max_lines, "%d", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SliderInt("Points per line", &sampling.samples_per_line, PatchSampling::minimum_count,
                     PatchSampling::max_samples, "%d", ImGuiSliderFlags_AlwaysClamp);
    ImGui::PopItemWidth();
    return sampling != before;
}
