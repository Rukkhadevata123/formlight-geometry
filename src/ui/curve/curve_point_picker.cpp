module;

#include <imgui.h>

module occt_lab.ui.curve.curve_editor;

import std;
import occt_lab.application.editable_curve;
import occt_lab.render.viewport.clip_transform;
import occt_lab.render.viewport.viewport_projection;

void CurveEditor::preview_point_picking(const EditableCurve &curve, const ClipTransform &transform,
                                        const bool scene_click)
{
    auto *viewport = ImGui::GetMainViewport();
    const occt_lab::render::ViewportProjection project{transform, *viewport, curve.current().framing_extent};
    auto &draw = *ImGui::GetBackgroundDrawList(viewport);
    const ImU32 color =
        creation_preset_ == control_points ? IM_COL32(116, 187, 226, 255) : IM_COL32(104, 226, 153, 255);

    const auto &io = ImGui::GetIO();
    if (!io.WantCaptureMouse && !ImGui::IsMouseDown(ImGuiMouseButton_Middle) && ImGui::IsMousePosValid())
    {
        const auto cursor = ImGui::GetMousePos();
        if (const auto hit = project.xy_plane_point(cursor))
        {
            if (scene_click)
            {
                creation_points_.push_back(*hit);
                refresh_creation_preview(curve);
            }
            else if (!creation_points_.empty())
            {
                project.draw_polyline(draw, std::array{creation_points_.back(), *hit}, color, 1);
            }
            draw.AddCircle(cursor, 5, color);
        }
        else if (scene_click)
        {
            creation_error_ = "No visible XY intersection here. Orbit or reset the view.";
        }
    }
    if (creation_preview_)
    {
        return; // draw_preview renders the sampled curve together with its control polygon.
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
    ImGui::TextUnformatted(creation_preset_ == control_points ? "Click control points on XY (Z = 0 mm)."
                                                              : "Click points in order on the XY plane (Z = 0 mm).");
    ImGui::Text("Points: %zu", creation_points_.size());
    if (draw_point_parameters())
    {
        refresh_creation_preview(curve);
    }
    ImGui::BeginDisabled(creation_points_.empty());
    if (ImGui::Button("Remove last"))
    {
        creation_points_.pop_back();
        refresh_creation_preview(curve);
    }
    ImGui::EndDisabled();
    draw_creation_fit_summary();
    ImGui::BeginDisabled(!creation_preview_);
    if (ImGui::Button("Create"))
    {
        const auto result = create_from_draft(curve, EditableCurve::Framing::keep_view);
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
