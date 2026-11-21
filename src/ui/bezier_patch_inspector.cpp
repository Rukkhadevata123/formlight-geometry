#include "bezier_patch_inspector.hpp"

#include "application/editable_patch.hpp"
#include "render/bezier_patch_preview.hpp"

#include <imgui.h>

#include <exception>

namespace
{
void draw_mesh_controls(PatchMeshControls &mesh, const bool show_report)
{
    ImGui::SeparatorText("Vulkan surface mesh");
    ImGui::Checkbox("Show GPU mesh", &mesh.visible);
    if (mesh.visible)
    {
        constexpr uint32_t minimum = 1;
        constexpr uint32_t maximum = occt_lab::geometry::PatchGrid::max_segments;
        const auto previous = mesh.grid;
        ImGui::SliderScalar("U subdivisions", ImGuiDataType_U32, &mesh.grid.u_segments, &minimum, &maximum, "%u",
                            ImGuiSliderFlags_AlwaysClamp);
        ImGui::SliderScalar("V subdivisions", ImGuiDataType_U32, &mesh.grid.v_segments, &minimum, &maximum, "%u",
                            ImGuiSliderFlags_AlwaysClamp);
        if (previous != mesh.grid)
        {
            mesh.report.clear();
        }
        mesh.compare_requested = ImGui::Button("Compare grid with OCCT");
    }
    if (show_report && !mesh.report.empty())
    {
        ImGui::TextUnformatted(mesh.report.c_str());
    }
}

void draw_surface_sample(const occt_lab::geometry::SurfaceSample &sample)
{
    const auto &[position, normal] = sample;
    ImGui::Text("Position (mm)   (%.5f, %.5f, %.5f)", position.x, position.y, position.z);
    ImGui::Text("Unit normal     (%.5f, %.5f, %.5f)", normal.x, normal.y, normal.z);
}
} // namespace

BezierPatchInspector::BezierPatchInspector(EditablePatch &model) : model_(model), draft_(model.current().control_net())
{
    update_samples();
}

std::size_t BezierPatchInspector::selected_pole() const
{
    return static_cast<std::size_t>(pole_v_ - 1) * EditablePatch::Patch::side_length +
           static_cast<std::size_t>(pole_u_ - 1);
}

void BezierPatchInspector::select_pole(const std::size_t index)
{
    constexpr auto side = EditablePatch::Patch::side_length;
    IM_ASSERT(index < draft_.size());
    pole_u_ = static_cast<int>(index % side) + 1;
    pole_v_ = static_cast<int>(index / side) + 1;
    reveal_selected_pole_ = true;
}

bool BezierPatchInspector::draw_pole_editor()
{
    if (reveal_selected_pole_)
    {
        ImGui::SetNextItemOpen(true);
        reveal_selected_pole_ = false;
    }
    if (!ImGui::CollapsingHeader("Control points"))
    {
        return false;
    }
    constexpr int side = static_cast<int>(EditablePatch::Patch::side_length);
    ImGui::PushItemWidth(11.0F * ImGui::GetFontSize());
    ImGui::SliderInt("Pole U", &pole_u_, 1, side, "%d", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SliderInt("Pole V", &pole_v_, 1, side, "%d", ImGuiSliderFlags_AlwaysClamp);
    const auto index = selected_pole();
    auto &[x, y, z] = draft_[index];
    ImGui::PushID(static_cast<int>(index));
    ImGui::PushItemFlag(ImGuiItemFlags_LiveEditOnInputScalar, true);
    constexpr auto flags = ImGuiInputTextFlags_ParseEmptyRefVal;
    const bool changed_x = ImGui::InputDouble("X (mm)", &x, 0, 0, "%.8g", flags);
    const bool changed_y = ImGui::InputDouble("Y (mm)", &y, 0, 0, "%.8g", flags);
    const bool changed_z = ImGui::InputDouble("Z (mm)", &z, 0, 0, "%.8g", flags);
    ImGui::PopItemFlag();
    ImGui::PopID();
    ImGui::PopItemWidth();
    if (changed_x || changed_y || changed_z)
    {
        edit_error_.clear();
    }

    const auto &applied = model_.current().control_net();
    const auto &point = applied[index];
    ImGui::Text("Applied: (%.8g, %.8g, %.8g) mm", point.x, point.y, point.z);
    ImGui::BeginDisabled(draft_ == applied);
    const bool apply = ImGui::Button("Apply");
    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
    {
        draft_ = applied;
        edit_error_.clear();
    }
    ImGui::EndDisabled();
    return apply;
}

void BezierPatchInspector::cancel_drag()
{
    if (tracking_pole_)
    {
        model_.discard_preview();
        tracking_pole_ = false;
        draft_ = model_.current().control_net();
        update_samples();
    }
}

void BezierPatchInspector::handle_viewport_input(const ClipTransform &transform, const bool pole_click,
                                                 PatchMeshControls &mesh)
{
    constexpr auto button = ImGuiMouseButton_Left;
    if (!tracking_pole_)
    {
        if (pole_click)
        {
            if (const auto hit = pick_patch_pole(model_, transform, ImGui::GetMousePos()))
            {
                select_pole(*hit);
                tracking_pole_ = draft_ == model_.current().control_net();
            }
        }
        return;
    }

    const bool released = ImGui::IsMouseReleased(button);
    if (ImGui::IsKeyPressed(ImGuiKey_Escape) || !ImGui::IsMousePosValid() || (!ImGui::IsMouseDown(button) && !released))
    {
        cancel_drag();
        return;
    }

    const auto index = selected_pole();
    const auto position = dragged_patch_pole_position(model_, transform, index, ImGui::GetMouseDragDelta(button));
    if (const auto changed = model_.preview_pole_move(index, position); !changed)
    {
        cancel_drag();
        edit_error_ = changed.error();
        return;
    }
    else if (*changed)
    {
        draft_[index] = position;
        update_samples();
    }
    if (released)
    {
        tracking_pole_ = false;
        if (model_.commit_preview())
        {
            refresh_after_edit(mesh);
        }
    }
}

void BezierPatchInspector::update_samples()
{
    sample_ = model_.displayed().evaluate(u_, v_);
    reference_sample_ = model_.reference().evaluate(u_, v_);
}

void BezierPatchInspector::draw_patch_commands(PatchMeshControls &mesh)
{
    if (draw_pole_editor())
    {
        if (const auto applied = model_.apply(draft_); !applied)
        {
            edit_error_ = applied.error();
        }
        else if (*applied)
        {
            refresh_after_edit(mesh);
        }
    }
    ImGui::BeginDisabled(draft_ != model_.current().control_net());
    const bool create = ImGui::Button("New planar patch");
    ImGui::SetItemTooltip("Replace the current patch with a 90 x 72 mm plane in XY.");
    ImGui::SameLine();
    ImGui::BeginDisabled(!model_.can_undo());
    const bool undo = ImGui::Button("Undo");
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!model_.can_redo());
    const bool redo = ImGui::Button("Redo");
    ImGui::EndDisabled();
    ImGui::EndDisabled();

    if (create || undo || redo)
    {
        try
        {
            bool changed = true;
            if (create)
            {
                changed = model_.create_plane();
            }
            else if (undo)
            {
                model_.undo();
            }
            else
            {
                model_.redo();
            }
            if (changed)
            {
                refresh_after_edit(mesh);
            }
        }
        catch (const std::exception &error)
        {
            edit_error_ = error.what();
        }
    }
    if (!edit_error_.empty())
    {
        ImGui::TextUnformatted(edit_error_.c_str());
    }
}

void BezierPatchInspector::refresh_after_edit(PatchMeshControls &mesh)
{
    draft_ = model_.current().control_net();
    update_samples();
    mesh.report.clear();
    sampling_failed_ = false;
    edit_error_.clear();
}

void BezierPatchInspector::draw(PatchMeshControls &mesh, const bool compute_supported)
{
    mesh.compare_requested = false;
    const auto *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({viewport->WorkPos.x + viewport->WorkSize.x - 16.0F, viewport->WorkPos.y + 16.0F},
                            ImGuiCond_FirstUseEver, {1.0F, 0.0F});
    if (ImGui::Begin("Bezier surface sample", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::BeginDisabled(tracking_pole_);
        draw_patch_commands(mesh);
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
            update_samples();
        }

        ImGui::SeparatorText("Bernstein evaluation");
        if (sample_)
        {
            draw_surface_sample(*sample_);
        }
        else if (sample_.error() == occt_lab::geometry::PatchError::DegenerateNormal)
        {
            ImGui::TextUnformatted("The surface normal is undefined at this parameter.");
        }
        else if (sample_.error() == occt_lab::geometry::PatchError::NonFiniteEvaluation)
        {
            ImGui::TextUnformatted("Bernstein evaluation exceeded the finite coordinate range.");
        }
        else
        {
            ImGui::TextUnformatted("Choose finite u and v values between 0 and 1.");
        }
        draw_reference();
        draw_sampling_controls();
        if (compute_supported)
        {
            draw_mesh_controls(mesh, !tracking_pole_);
        }
        ImGui::EndDisabled();
    }
    ImGui::End();
}

void BezierPatchInspector::draw_reference() const
{
    ImGui::SeparatorText("OCCT reference");
    if (!reference_sample_)
    {
        ImGui::TextUnformatted(reference_sample_.error().c_str());
        return;
    }
    draw_surface_sample(*reference_sample_);
    if (sample_)
    {
        const auto [distance_mm, angle_degrees] = compare_surface_samples(*sample_, *reference_sample_);
        ImGui::Text("Position distance: %.3g mm", distance_mm);
        ImGui::Text("Normal angle: %.3g deg", angle_degrees);
    }
}

void BezierPatchInspector::draw_sampling_controls()
{
    using occt_lab::geometry::PatchSampling;
    auto sampling = model_.sampling();
    ImGui::SeparatorText("Isoline sampling");
    ImGui::PushItemWidth(11.0F * ImGui::GetFontSize());
    ImGui::SliderInt("Constant u lines", &sampling.constant_u_lines, PatchSampling::minimum_count,
                     PatchSampling::max_lines, "%d", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SliderInt("Constant v lines", &sampling.constant_v_lines, PatchSampling::minimum_count,
                     PatchSampling::max_lines, "%d", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SliderInt("Points per line", &sampling.samples_per_line, PatchSampling::minimum_count,
                     PatchSampling::max_samples, "%d", ImGuiSliderFlags_AlwaysClamp);
    ImGui::PopItemWidth();
    if (sampling != model_.sampling())
    {
        sampling_failed_ = !model_.resample(sampling);
    }
    if (sampling_failed_)
    {
        ImGui::TextUnformatted("Could not sample the isolines. The previous settings are still in use.");
    }
}
