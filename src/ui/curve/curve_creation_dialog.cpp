#include "curve_editor.hpp"

#include <imgui.h>

#include <algorithm>

namespace
{
enum CreationPreset
{
    custom_line,
    cubic_bezier,
    interpolate_points
};

bool draw_line_parameters(LineCurveParameters &parameters)
{
    auto &[count, degree] = parameters;
    ImGui::TextUnformatted("Start with evenly spaced control points along X (90 mm).");
    constexpr auto flags = ImGuiSliderFlags_AlwaysClamp;
    const bool count_changed =
        ImGui::SliderInt("Poles", &count, LineCurveParameters::min_poles, LineCurveParameters::max_poles, "%d", flags);
    const int limit = LineCurveParameters::degree_limit(count);
    degree = std::min(degree, limit);
    ImGui::BeginDisabled(limit == 1);
    const bool degree_changed = ImGui::SliderInt("Degree", &degree, 1, limit, "%d", flags);
    ImGui::EndDisabled();
    return count_changed || degree_changed;
}

bool draw_point_table(std::vector<occt_lab::geometry::Vec3> &points)
{
    ImGui::TextUnformatted("The curve passes through these points in row order.");
    const float em = ImGui::GetFontSize();
    const float height = 7 * ImGui::GetFrameHeightWithSpacing();
    std::optional<std::size_t> remove;
    bool changed = false;
    if (ImGui::BeginTable("Data points", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, {31 * em, height}))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Point", ImGuiTableColumnFlags_WidthFixed);
        for (const char *label : {"X (mm)", "Y (mm)", "Z (mm)"})
        {
            ImGui::TableSetupColumn(label, ImGuiTableColumnFlags_WidthStretch);
        }
        ImGui::TableSetupColumn("##actions", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight());
        ImGui::TableHeadersRow();
        ImGui::PushItemFlag(ImGuiItemFlags_LiveEditOnInputScalar, true);
        for (std::size_t i = 0; i < points.size(); ++i)
        {
            ImGui::PushID(static_cast<int>(i));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%zu", i + 1);
            auto &[x, y, z] = points[i];
            for (double *coordinate : {&x, &y, &z})
            {
                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-1);
                ImGui::PushID(ImGui::TableGetColumnIndex());
                if (ImGui::InputDouble("##coordinate", coordinate, 0, 0, "%.12g"))
                {
                    changed = true;
                }
                ImGui::PopID();
            }
            ImGui::TableNextColumn();
            ImGui::BeginDisabled(points.size() <= 2);
            if (ImGui::SmallButton("-"))
            {
                remove = i;
            }
            ImGui::SetItemTooltip("Remove this point");
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        ImGui::PopItemFlag();
        ImGui::EndTable();
    }
    if (remove)
    {
        points.erase(points.begin() + static_cast<std::ptrdiff_t>(*remove));
        changed = true;
    }
    if (ImGui::Button("Add point"))
    {
        points.push_back({});
        changed = true;
    }
    return changed;
}

auto create_curve(EditableCurve &curve, const int preset, const LineCurveParameters &line,
                  const std::span<const occt_lab::geometry::Vec3> points) -> std::expected<void, std::string>
{
    switch (preset)
    {
    case custom_line:
        return curve.create_line(line);
    case cubic_bezier:
        return curve.create_cubic_bezier();
    case interpolate_points:
        return curve.create_interpolated_curve(points);
    default:
        return std::unexpected("Unknown curve preset.");
    }
}
} // namespace

void CurveEditor::draw_creation_dialog(EditableCurve &curve)
{
    constexpr char title[] = "New curve";
    if (ImGui::Button("New curve..."))
    {
        creation_parameters_ = {};
        creation_points_ = {{-45, -10, 0}, {-20, 28, 8}, {18, -18, 16}, {45, 12, 0}};
        creation_preset_ = custom_line;
        creation_error_.clear();
        ImGui::OpenPopup(title);
    }
    if (!ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        return;
    }
    ImGui::PushItemWidth(12.0F * ImGui::GetFontSize());
    const bool preset_changed =
        ImGui::Combo("Preset", &creation_preset_, "Custom line\0Cubic Bezier\0Interpolate through points\0");
    bool parameters_changed = false;
    switch (creation_preset_)
    {
    case custom_line:
        parameters_changed = draw_line_parameters(creation_parameters_);
        break;
    case cubic_bezier:
        ImGui::TextUnformatted("Four control points form a cubic arc in the XY plane.");
        break;
    case interpolate_points:
        parameters_changed = draw_point_table(creation_points_);
        if (ImGui::Button("Start picking on XY plane..."))
        {
            creation_points_.clear();
            creation_error_.clear();
            picking_points_ = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemTooltip("Start an empty point list on the world XY plane (Z = 0 mm).");
        break;
    }
    ImGui::PopItemWidth();
    if (preset_changed || parameters_changed)
    {
        creation_error_.clear();
    }

    if (ImGui::Button("Create"))
    {
        const auto result = create_curve(curve, creation_preset_, creation_parameters_, creation_points_);
        if (result)
        {
            finish_curve_operation(curve, result);
            ImGui::CloseCurrentPopup();
        }
        else
        {
            creation_error_ = result.error();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape))
    {
        ImGui::CloseCurrentPopup();
    }
    if (!creation_error_.empty())
    {
        ImGui::TextUnformatted(creation_error_.c_str());
    }
    ImGui::EndPopup();
}
