#include "bezier_patch_inspector.hpp"

#include <imgui.h>

BezierPatchInspector::BezierPatchInspector(const occt_lab::geometry::CubicBezierPatch &patch)
    : patch_(patch), sample_(patch_.evaluate(u_, v_))
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
            sample_ = patch_.evaluate(u_, v_);
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
    }
    ImGui::End();
}
