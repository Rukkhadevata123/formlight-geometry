module;

#include <imgui.h>

export module occt_lab.ui.curve.fit_residuals;

import std;
import occt_lab.geometry.curves.bspline_curve;
import occt_lab.render.preview.curve_colors;

using occt_lab::geometry::SampledCurve;

namespace
{
void show_point_tooltip(const SampledCurve &curve, const std::size_t index)
{
    if (ImGui::BeginTooltip())
    {
        const auto &point = curve.data_points[index];
        ImGui::Text("Point %zu   u = %.8g", index + 1, point.parameter);
        if (curve.approximation)
        {
            ImGui::Text("Fit: %.3g mm", point.residual);
            ImGui::Text("Interpolant: %.3g mm", curve.approximation->interpolated_residuals[index]);
        }
        else
        {
            ImGui::Text("Interpolant: %.3g mm", point.residual);
        }
        ImGui::EndTooltip();
    }
}

void draw_residual_plot(const SampledCurve &curve, const double peak)
{
    const float em = ImGui::GetFontSize();
    const auto top_label = std::format("{:.3g}", peak);
    const auto origin = ImGui::GetCursorScreenPos();
    // Keep the parameter axis in place as the residuals change magnitude.
    const float label_width = ImGui::CalcTextSize("9.99e-308").x;
    const ImVec2 upper_left{origin.x + label_width + 0.5F * em, origin.y + 0.5F * em};
    const ImVec2 lower_right{upper_left.x + 12.0F * em, upper_left.y + 4.0F * em};
    ImGui::Dummy({lower_right.x - origin.x + 0.5F * em, lower_right.y - origin.y + 1.4F * em});
    if (!ImGui::IsItemVisible())
    {
        return;
    }

    auto &draw = *ImGui::GetWindowDrawList();
    const auto text_color = ImGui::GetColorU32(ImGuiCol_Text);
    const auto axis_color = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    draw.AddRectFilled(upper_left, lower_right, ImGui::GetColorU32(ImGuiCol_FrameBg));
    draw.AddLine({upper_left.x, lower_right.y}, lower_right, axis_color);
    draw.AddText({origin.x, lower_right.y - 0.5F * em}, text_color, "0");
    if (peak > 0.0)
    {
        draw.AddText(origin, text_color, top_label.c_str());
    }
    const auto x_at = [&](const double u) { return std::lerp(upper_left.x, lower_right.x, static_cast<float>(u)); };
    const auto y_at = [&](const double residual)
    {
        // Normalize in double before converting to pixels, including roundoff-sized residuals.
        const double fraction = peak > 0.0 ? residual / peak : 0.0;
        return std::lerp(lower_right.y, upper_left.y, static_cast<float>(fraction));
    };
    const float label_y = lower_right.y + 0.3F * em;
    const auto x_label = [&](const double u, const char *label)
    { draw.AddText({x_at(u) - 0.5F * ImGui::CalcTextSize(label).x, label_y}, text_color, label); };
    x_label(0.0, "0");
    x_label(0.5, "Chord parameter u");
    x_label(1.0, "1");

    const float radius = 0.25F * em;
    if (ImGui::IsItemHovered() && ImGui::IsMouseHoveringRect({upper_left.x - radius, upper_left.y - radius},
                                                             {lower_right.x + radius, lower_right.y + radius}))
    {
        const double u = (ImGui::GetMousePos().x - upper_left.x) / (lower_right.x - upper_left.x);
        const auto nearest = std::ranges::min_element(curve.data_points, {},
                                                      [u](const auto &point) { return std::abs(point.parameter - u); });
        const float x = x_at(nearest->parameter);
        draw.AddLine({x, upper_left.y}, {x, lower_right.y}, axis_color);
        show_point_tooltip(curve, static_cast<std::size_t>(nearest - curve.data_points.begin()));
    }
    for (std::size_t i = 0; i < curve.data_points.size(); ++i)
    {
        const auto &point = curve.data_points[i];
        const float x = x_at(point.parameter);
        draw.AddCircleFilled({x, y_at(point.residual)}, 0.14F * em, curve_colors::current);
        if (curve.approximation)
        {
            const double reference = curve.approximation->interpolated_residuals[i];
            draw.AddCircle({x, y_at(reference)}, radius, curve_colors::interpolant_reference, 0, 0.09F * em);
        }
    }
}
} // namespace

export void draw_fit_residuals(const SampledCurve &curve)
{
    const auto &points = curve.data_points;
    if (points.empty())
    {
        return;
    }
    const auto largest = std::ranges::max_element(points, {}, &occt_lab::geometry::CurveDataPoint::residual);
    double peak = largest->residual;
    ImGui::Text("Green diamonds: %zu data points", points.size());
    if (curve.approximation)
    {
        const auto &comparison = *curve.approximation;
        ImGui::Text("Applied target: %.8g mm", comparison.requested_tolerance);
        ImGui::Text("Max at chord parameters (mm)");
        const double reference_peak = std::ranges::max(comparison.interpolated_residuals);
        ImGui::Text("Gold fit: %.3g   Purple interpolant: %.3g", largest->residual, reference_peak);
        peak = std::max(peak, reference_peak);
    }
    else
    {
        ImGui::Text("Max point residual: %.3g mm", largest->residual);
    }
    if (ImGui::TreeNodeEx("Point residuals (mm)", ImGuiTreeNodeFlags_DefaultOpen))
    {
        draw_residual_plot(curve, peak);
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Chord parameters and residuals"))
    {
        if (curve.approximation)
        {
            ImGui::TextUnformatted("Point    u      Fit / interpolant (mm)");
        }
        for (std::size_t i = 0; i < points.size(); ++i)
        {
            if (curve.approximation)
            {
                ImGui::Text("%zu   %.8g   %.3g / %.3g", i + 1, points[i].parameter, points[i].residual,
                            curve.approximation->interpolated_residuals[i]);
            }
            else
            {
                ImGui::Text("%zu   u = %.8g   %.3g mm", i + 1, points[i].parameter, points[i].residual);
            }
        }
        ImGui::TreePop();
    }
}
