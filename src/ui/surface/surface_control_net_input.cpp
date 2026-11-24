module;

#include <imgui.h>

module occt_lab.ui.surface.nurbs_pole_inspector;

import std;
import occt_lab.geometry.vec3;
import occt_lab.render.preview.surface.nurbs_surface_preview;
import occt_lab.render.viewport.clip_transform;

bool NurbsPoleInspector::draw_net_table()
{
    const int columns = net_parameters_.u_poles;
    const float em = ImGui::GetFontSize();
    const float height = 7 * ImGui::GetFrameHeightWithSpacing();
    bool changed = false;
    if (ImGui::BeginTable("Control net", 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, {36 * em, height}))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("U", ImGuiTableColumnFlags_WidthFixed, 2 * em);
        ImGui::TableSetupColumn("V", ImGuiTableColumnFlags_WidthFixed, 2 * em);
        for (const char *label : {"X (mm)", "Y (mm)", "Z (mm)", "Weight"})
        {
            ImGui::TableSetupColumn(label, ImGuiTableColumnFlags_WidthStretch);
        }
        ImGui::TableHeadersRow();
        ImGui::PushItemFlag(ImGuiItemFlags_LiveEditOnInputScalar, true);
        ImGuiListClipper clipper;
        clipper.Begin(columns * net_parameters_.v_poles);
        while (clipper.Step())
        {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
            {
                const int u = i % columns, v = i / columns;
                auto &[position, weight] = creation_net_[v][u];
                ImGui::PushID(v);
                ImGui::PushID(u);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%d", u + 1);
                ImGui::TableNextColumn();
                ImGui::Text("%d", v + 1);
                for (double *value : {&position.x, &position.y, &position.z, &weight})
                {
                    ImGui::TableNextColumn();
                    ImGui::PushID(ImGui::TableGetColumnIndex());
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputDouble("##value", value, 0, 0, "%.12g"))
                    {
                        changed = true;
                    }
                    if (ImGui::IsItemActive())
                    {
                        net_selected_ = static_cast<std::size_t>(i);
                    }
                    ImGui::PopID();
                }
                ImGui::PopID();
                ImGui::PopID();
            }
        }
        ImGui::PopItemFlag();
        ImGui::EndTable();
    }
    return changed;
}

void NurbsPoleInspector::draw_net_picker(const ClipTransform &transform, const bool scene_click)
{
    auto &[frame, points] = *picked_net_;
    const auto columns = static_cast<std::size_t>(net_parameters_.u_poles);
    const auto count = columns * net_parameters_.v_poles;
    const auto project = nurbs_surface_projection(transform, frame);
    auto &draw = *ImGui::GetBackgroundDrawList(ImGui::GetMainViewport());
    constexpr ImU32 blue = IM_COL32(116, 187, 226, 255);
    const auto draw_neighbors = [&](std::size_t i, const occt_lab::geometry::Vec3 &point)
    {
        if (i % columns != 0)
        {
            project.draw_polyline(draw, std::array{points[i - 1], point}, blue, 1.5F);
        }
        if (i >= columns)
        {
            project.draw_polyline(draw, std::array{points[i - columns], point}, blue, 1.5F);
        }
    };
    if (points.size() < count && !ImGui::GetIO().WantCaptureMouse && ImGui::IsMousePosValid() &&
        !ImGui::IsMouseDown(ImGuiMouseButton_Middle))
    {
        const auto cursor = ImGui::GetMousePos();
        if (const auto hit = project.xy_plane_point(cursor))
        {
            draw_neighbors(points.size(), *hit);
            draw.AddCircle(cursor, 5, blue);
            if (scene_click)
            {
                points.push_back(*hit);
                creation_error_.clear();
            }
        }
        else if (scene_click)
        {
            creation_error_ = "No visible XY intersection here. Orbit or reset the view.";
        }
    }
    for (std::size_t i = 0; i < points.size(); ++i)
    {
        draw_neighbors(i, points[i]);
        if (const auto pixel = project.visible_point(points[i]))
        {
            draw.AddCircleFilled(*pixel, 4, blue);
        }
    }
    bool done = false;
    if (begin_inspector_window())
    {
        ImGui::Text("XY plane (Z = 0 mm)    %zu / %zu", points.size(), count);
        if (points.size() < count)
        {
            ImGui::Text("Next pole: U %zu   V %zu", points.size() % columns + 1, points.size() / columns + 1);
        }
        ImGui::BeginDisabled(points.empty());
        if (ImGui::Button("Remove last"))
        {
            points.pop_back();
            creation_error_.clear();
        }
        ImGui::EndDisabled();
        ImGui::BeginDisabled(points.size() != count);
        if (ImGui::Button("Use positions"))
        {
            net_frame_ = frame;
            for (std::size_t i = 0; i < count; ++i)
            {
                creation_net_[i / columns][i % columns].position = points[i];
            }
            done = true;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel picking"))
        {
            done = true;
        }
        if (!creation_error_.empty())
        {
            ImGui::TextUnformatted(creation_error_.c_str());
        }
    }
    ImGui::End();
    if (done || ImGui::IsKeyPressed(ImGuiKey_Escape))
    {
        picked_net_.reset();
        resume_net_ = true;
    }
}
