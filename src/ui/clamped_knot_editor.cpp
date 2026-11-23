#include "clamped_knot_editor.hpp"

#include <imgui.h>

#include <algorithm>
#include <numeric>

namespace
{
using KnotList = std::vector<occt_lab::geometry::BSplineKnot>;

bool draw_knot_table(KnotList &knots, const int poles, const int degree)
{
    bool changed = false;
    std::optional<std::size_t> remove_index;
    const float row_height = ImGui::GetFrameHeightWithSpacing();
    const float height = row_height * static_cast<float>(1 + std::min<std::size_t>(knots.size(), 4));
    if (ImGui::BeginTable("Knots", 3, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg,
                          {19.0F * ImGui::GetFontSize(), height}))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Knot", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Multiplicity", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("##actions", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight());
        ImGui::TableHeadersRow();
        for (std::size_t i = 0; i < knots.size(); ++i)
        {
            auto &[value, multiplicity] = knots[i];
            ImGui::PushID(static_cast<int>(i));
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputDouble("##value", &value, 0, 0, "%.12g"))
            {
                changed = true;
            }
            ImGui::TableSetColumnIndex(1);
            if (i == 0 || i + 1 == knots.size())
            {
                ImGui::Text("%d", multiplicity);
            }
            else
            {
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderInt("##multiplicity", &multiplicity, 1, degree, "%d", ImGuiSliderFlags_AlwaysClamp))
                {
                    changed = true;
                }
                ImGui::SetItemTooltip("Guaranteed continuity at this knot: C%d (degree - multiplicity).",
                                      degree - multiplicity);
                ImGui::TableSetColumnIndex(2);
                if (ImGui::SmallButton("-"))
                {
                    remove_index = i;
                }
                ImGui::SetItemTooltip("Remove interior knot");
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (remove_index)
    {
        knots.erase(knots.begin() + static_cast<KnotList::difference_type>(*remove_index));
        changed = true;
    }

    ImGui::BeginDisabled(knots.size() >= static_cast<std::size_t>(poles - degree + 1));
    if (ImGui::Button("Add interior knot"))
    {
        const auto last = knots.end() - 1;
        const double value = std::midpoint((last - 1)->value, last->value);
        knots.insert(last, {.value = value, .multiplicity = 1});
        changed = true;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    const int total = std::accumulate(knots.begin(), knots.end(), 0,
                                      [](int sum, const auto &knot) { return sum + knot.multiplicity; });
    ImGui::Text("Total %d / %d", total, poles + degree + 1);
    return changed;
}
} // namespace

bool draw_custom_knots(const char *label, const int poles, const int degree, std::optional<KnotList> &knots)
{
    bool custom = knots.has_value();
    const bool mode_changed = ImGui::Checkbox(label, &custom);
    if (!custom)
    {
        knots.reset();
        return mode_changed;
    }
    if (!knots)
    {
        knots = occt_lab::geometry::make_uniform_clamped_knots(poles, degree);
    }
    const bool degree_changed = knots->front().multiplicity != degree + 1;
    if (degree_changed)
    {
        knots->front().multiplicity = degree + 1;
        knots->back().multiplicity = degree + 1;
        for (std::size_t i = 1; i + 1 < knots->size(); ++i)
        {
            (*knots)[i].multiplicity = std::min((*knots)[i].multiplicity, degree);
        }
    }

    ImGui::PushID(label);
    ImGui::PushItemFlag(ImGuiItemFlags_LiveEditOnInputScalar, true);
    const bool table_changed = draw_knot_table(*knots, poles, degree);
    ImGui::PopItemFlag();
    ImGui::PopID();
    return mode_changed || degree_changed || table_changed;
}
