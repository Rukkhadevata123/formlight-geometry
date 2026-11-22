#include "curve_editor.hpp"

#include "render/viewport/viewport_projection.hpp"

#include <imgui.h>

#include <array>

void CurveEditor::preview_point_picking(const EditableCurve &curve, const ClipTransform &transform,
                                        const bool scene_click)
{
    auto *viewport = ImGui::GetMainViewport();
    const occt_lab::render::ViewportProjection project{transform, *viewport, curve.current().framing_extent};
    auto &draw = *ImGui::GetBackgroundDrawList(viewport);
    constexpr ImU32 color = IM_COL32(104, 226, 153, 255);

    const auto &io = ImGui::GetIO();
    if (!io.WantCaptureMouse && !ImGui::IsMouseDown(ImGuiMouseButton_Middle) && ImGui::IsMousePosValid())
    {
        const auto cursor = ImGui::GetMousePos();
        if (const auto hit = project.xy_plane_point(cursor))
        {
            if (scene_click)
            {
                creation_points_.push_back(*hit);
                creation_error_.clear();
            }
            else if (!creation_points_.empty())
            {
                project.draw_polyline(draw, std::array{creation_points_.back(), *hit}, IM_COL32(104, 226, 153, 140), 1);
            }
            draw.AddCircle(cursor, 5, color);
        }
        else if (scene_click)
        {
            creation_error_ = "No visible XY intersection here. Orbit or reset the view.";
        }
    }
    project.draw_polyline(draw, creation_points_, color, 2);
    for (const auto &point : creation_points_)
    {
        if (const auto pixel = project.visible_point(point))
        {
            draw.AddCircleFilled(*pixel, 4, color);
        }
    }
}

void CurveEditor::draw_point_picker(EditableCurve &curve)
{
    ImGui::TextUnformatted("Click points in order on the XY plane (Z = 0 mm).");
    ImGui::Text("Points: %zu", creation_points_.size());
    ImGui::BeginDisabled(creation_points_.empty());
    if (ImGui::Button("Remove last"))
    {
        creation_points_.pop_back();
        creation_error_.clear();
    }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(creation_points_.size() < 2);
    if (ImGui::Button("Create"))
    {
        const auto result = curve.create_interpolated_curve(creation_points_, EditableCurve::Framing::keep_view);
        if (result)
        {
            finish_curve_operation(curve, result);
            picking_points_ = false;
        }
        else
        {
            creation_error_ = result.error();
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel (Esc)"))
    {
        cancel_point_picking();
    }
    if (!creation_error_.empty())
    {
        ImGui::TextUnformatted(creation_error_.c_str());
    }
}
