module;

#include <imgui.h>

module formlight_geometry.ui.curve.curve_editor;

import std;
import formlight_geometry.application.editable_curve;
import formlight_geometry.application.nurbs_curve_creation;
import formlight_geometry.geometry.curves.bspline_curve;
import formlight_geometry.ui.clamped_knot_editor;

namespace
{
bool draw_line_parameters(LineCurveParameters &parameters)
{
    auto &[count, degree, knots] = parameters;
    ImGui::TextUnformatted("Start with evenly spaced control points along X (90 mm).");
    constexpr auto flags = ImGuiSliderFlags_AlwaysClamp;
    const bool count_changed =
        ImGui::SliderInt("Poles", &count, LineCurveParameters::min_poles, LineCurveParameters::max_poles, "%d", flags);
    const int limit = formlight_geometry::geometry::curve_degree_limit(count);
    degree = std::min(degree, limit);
    ImGui::BeginDisabled(limit == 1);
    const bool degree_changed = ImGui::SliderInt("Degree", &degree, 1, limit, "%d", flags);
    ImGui::EndDisabled();
    const bool knots_changed = draw_custom_knots("Custom knots", count, degree, knots);
    return count_changed || degree_changed || knots_changed;
}

bool draw_point_table(std::vector<formlight_geometry::geometry::Vec3> &points)
{
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

} // namespace

bool CurveEditor::draw_control_parameters()
{
    const int limit = formlight_geometry::geometry::curve_degree_limit(creation_points_.size());
    if (limit < 1)
    {
        return false;
    }
    // Picking starts with an empty polygon. Keep the configured knots until enough poles are back.
    if (picking_points_ && control_knots_ && limit < control_degree_)
    {
        ImGui::Text("Degree %d requires at least %d control points.", control_degree_, control_degree_ + 1);
        if (ImGui::Button("Use uniform knots"))
        {
            control_knots_.reset();
            return true;
        }
        return false;
    }
    int degree = std::min(control_degree_, limit);
    ImGui::BeginDisabled(limit == 1);
    const bool changed = ImGui::SliderInt("Degree", &degree, 1, limit, "%d", ImGuiSliderFlags_AlwaysClamp);
    ImGui::EndDisabled();
    const bool knots_changed =
        draw_custom_knots("Custom knots", static_cast<int>(creation_points_.size()), degree, control_knots_);
    if (changed || control_knots_)
    {
        control_degree_ = degree;
    }
    return changed || knots_changed;
}

bool CurveEditor::draw_point_parameters()
{
    if (creation_preset_ == control_points)
    {
        return draw_control_parameters();
    }
    if (creation_preset_ == approximate_points)
    {
        ImGui::PushItemFlag(ImGuiItemFlags_LiveEditOnInputScalar, true);
        const bool changed = ImGui::InputDouble("Target tolerance (mm)", &creation_tolerance_, 0, 0, "%.8g");
        ImGui::PopItemFlag();
        return changed;
    }
    return false;
}

void CurveEditor::draw_creation_fit_summary() const
{
    if (creation_preview_ && !creation_preview_->data_points.empty())
    {
        const auto largest =
            std::ranges::max_element(creation_preview_->data_points, {}, &formlight_geometry::geometry::CurveDataPoint::residual);
        ImGui::Text("Control points: %zu   Max point residual: %.4g mm", creation_preview_->definition.poles.size(),
                    largest->residual);
        if (creation_preview_->approximation)
        {
            ImGui::TextUnformatted("Gold: fit   Purple: interpolant");
        }
    }
}

void CurveEditor::refresh_creation_preview(const EditableCurve &curve)
{
    creation_preview_.reset();
    creation_error_.clear();
    if (creation_points_.size() < 2)
    {
        return;
    }
    try
    {
        switch (creation_preset_)
        {
        case approximate_points:
            creation_preview_ = formlight_geometry::geometry::approximate_curve(creation_points_, creation_tolerance_);
            break;
        case interpolate_points:
            creation_preview_ = formlight_geometry::geometry::interpolate_curve(creation_points_);
            break;
        case control_points:
        {
            const int limit = formlight_geometry::geometry::curve_degree_limit(creation_points_.size());
            if (picking_points_ && control_knots_ && control_degree_ > limit)
            {
                return;
            }
            const int degree = std::min(control_degree_, limit);
            creation_preview_ =
                sample_curve(formlight_geometry::geometry::make_clamped_curve(creation_points_, degree, control_knots_));
            break;
        }
        default:
            return;
        }
        if (picking_points_)
        {
            creation_preview_->framing_extent = curve.current().framing_extent;
        }
    }
    catch (const std::exception &error)
    {
        creation_error_ = error.what();
    }
}

auto CurveEditor::create_from_draft(EditableCurve &curve, const EditableCurve::Framing framing)
    -> std::expected<void, std::string>
{
    switch (creation_preset_)
    {
    case custom_line:
        return curve.create_line(creation_parameters_);
    case cubic_bezier:
        return curve.create_cubic_bezier();
    case interpolate_points:
    case approximate_points:
    case control_points:
        if (!creation_preview_)
        {
            return std::unexpected("A valid point preview is required.");
        }
        curve.replace_with_created_curve(std::move(*creation_preview_), framing);
        creation_preview_.reset();
        if (creation_preset_ == approximate_points)
        {
            settings_.fit_tolerance = creation_tolerance_;
        }
        return {};
    default:
        return std::unexpected("Unknown curve preset.");
    }
}

void CurveEditor::draw_creation_dialog(EditableCurve &curve)
{
    constexpr char title[] = "New curve";
    if (ImGui::Button("New curve..."))
    {
        creation_parameters_ = {};
        creation_tolerance_ = settings_.fit_tolerance;
        control_degree_ = 3;
        control_knots_.reset();
        creation_preview_.reset();
        creation_points_ = {{-45, -10, 0}, {-20, 28, 8}, {18, -18, 16}, {45, 12, 0}};
        creation_preset_ = custom_line;
        creation_error_.clear();
        ImGui::OpenPopup(title);
    }
    if (!ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        creation_preview_.reset();
        return;
    }
    ImGui::PushItemWidth(12.0F * ImGui::GetFontSize());
    const bool preset_changed =
        ImGui::Combo("Preset", &creation_preset_,
                     "Custom line\0Cubic Bezier\0Interpolate through points\0From control points\0"
                     "Approximate points\0");
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
    case approximate_points:
    case control_points:
        if (creation_preset_ == control_points)
        {
            ImGui::TextUnformatted("Rows form the control polygon in order.");
        }
        else if (creation_preset_ == interpolate_points)
        {
            ImGui::TextUnformatted("The curve passes through these points in row order.");
        }
        parameters_changed = draw_point_table(creation_points_);
        if (draw_point_parameters())
        {
            parameters_changed = true;
        }
        if (ImGui::Button("Start picking on XY plane..."))
        {
            creation_points_.clear();
            creation_error_.clear();
            creation_preview_.reset();
            picking_points_ = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemTooltip("Start an empty point list on the world XY plane (Z = 0 mm).");
        break;
    }
    ImGui::PopItemWidth();
    if (preset_changed || parameters_changed)
    {
        refresh_creation_preview(curve);
    }

    draw_creation_fit_summary();
    bool can_create = creation_preset_ == custom_line || creation_preset_ == cubic_bezier || creation_preview_;
    if (creation_preset_ == custom_line && creation_parameters_.knots)
    {
        const auto valid = formlight_geometry::geometry::validate_clamped_knots(
            *creation_parameters_.knots, creation_parameters_.degree, creation_parameters_.pole_count);
        if (!valid)
        {
            creation_error_ = valid.error();
            can_create = false;
        }
    }
    ImGui::BeginDisabled(!can_create);
    if (ImGui::Button("Create"))
    {
        const auto result = create_from_draft(curve, EditableCurve::Framing::fit_curve);
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
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape))
    {
        creation_preview_.reset();
        ImGui::CloseCurrentPopup();
    }
    if (!creation_error_.empty())
    {
        ImGui::TextUnformatted(creation_error_.c_str());
    }
    ImGui::EndPopup();
}
