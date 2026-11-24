module;

#include <imgui.h>

export module occt_lab.ui.clamped_knot_editor;

import std;
import occt_lab.geometry.bspline_knot;

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

export void draw_knot_inspector(const char *label, const std::span<const occt_lab::geometry::BSplineKnot> knots,
                                const int degree)
{
    if (!ImGui::TreeNode(label))
    {
        return;
    }
    constexpr int visible_rows = 6;
    const int count = static_cast<int>(knots.size());
    const bool scroll = count > visible_rows;
    const float row_height = ImGui::GetTextLineHeight() + 2.0F * ImGui::GetStyle().CellPadding.y;
    const ImVec2 size{23.0F * ImGui::GetFontSize(), scroll ? (1 + visible_rows) * row_height : 0.0F};
    const auto flags = ImGuiTableFlags_RowBg | (scroll ? ImGuiTableFlags_ScrollY : ImGuiTableFlags_None);
    if (ImGui::BeginTable("##knots", 3, flags, size))
    {
        ImGui::TableSetupColumn("Knot");
        ImGui::TableSetupColumn("Multiplicity");
        ImGui::TableSetupColumn("Continuity");
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();
        ImGuiListClipper clipper;
        clipper.Begin(count);
        while (clipper.Step())
        {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
            {
                const auto [value, multiplicity] = knots[i];
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%.12g", value);
                ImGui::TableNextColumn();
                ImGui::Text("%d", multiplicity);
                ImGui::TableNextColumn();
                if (i == 0 || i + 1 == count)
                {
                    ImGui::TextUnformatted("Endpoint");
                }
                else
                {
                    ImGui::Text("At least C%d", degree - multiplicity);
                }
            }
        }
        ImGui::EndTable();
    }
    ImGui::TreePop();
}

// Returns true when the custom table or the choice of knot mode changes.
export bool draw_custom_knots(const char *label, const int poles, const int degree, std::optional<KnotList> &knots)
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
