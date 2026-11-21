#include "nurbs_pole_inspector.hpp"
#include "isoline_sampling.hpp"
#include "pole_drag.hpp"
#include "render/viewport/viewport_projection.hpp"

#include <imgui.h>

#include <limits>

NurbsPoleInspector::NurbsPoleInspector(const EditableNurbsSurface &model)
{
    reset_draft(model);
}

void NurbsPoleInspector::reset_draft(const EditableNurbsSurface &model)
{
    const auto &definition = model.current().definition;
    poles_.reserve(definition.poles.size());
    weights_.reserve(definition.weights.size());
    if (draft_columns_ != definition.u.pole_count || poles_.size() != definition.poles.size())
    {
        selected_pole_ = 0;
    }
    draft_columns_ = definition.u.pole_count;
    poles_ = definition.poles;
    weights_ = definition.weights;
    error_.clear();
    sampling_error_.clear();
}

bool NurbsPoleInspector::has_edits(const EditableNurbsSurface::Definition &surface) const
{
    return poles_ != surface.poles || weights_ != surface.weights;
}

void NurbsPoleInspector::cancel_drag(EditableNurbsSurface &model)
{
    if (tracking_pole_)
    {
        model.discard_preview();
        tracking_pole_ = false;
        reset_draft(model);
    }
}

void NurbsPoleInspector::handle_viewport_input(EditableNurbsSurface &model,
                                               const occt_lab::render::ViewportProjection &project,
                                               const bool pole_click)
{
    const auto &surface = model.current().definition;
    if (!tracking_pole_)
    {
        if (pole_click)
        {
            if (const auto hit = project.pick_pole(surface.poles, ImGui::GetMousePos()))
            {
                selected_pole_ = *hit;
                tracking_pole_ = !has_edits(surface);
            }
        }
        return;
    }
    const auto motion = read_pole_drag();
    if (!motion)
    {
        cancel_drag(model);
        return;
    }
    const auto position = project.dragged_position(surface.poles.at(selected_pole_), motion->delta);
    if (const auto result = model.preview_pole_move(selected_pole_, position); !result)
    {
        cancel_drag(model);
        error_ = result.error();
        return;
    }
    poles_[selected_pole_] = position;
    if (motion->released)
    {
        model.commit_preview();
        tracking_pole_ = false;
        reset_draft(model);
    }
}

const occt_lab::geometry::SurfaceProbe *NurbsPoleInspector::draw(EditableNurbsSurface &model)
{
    const auto *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({viewport->WorkPos.x + viewport->WorkSize.x - 16.0F, viewport->WorkPos.y + 16.0F},
                            ImGuiCond_FirstUseEver, {1.0F, 0.0F});
    constexpr char title[] = "NURBS control point";
    const auto &style = ImGui::GetStyle();
    // Reserve a frame for the collapse button even when the selected pole has short coordinates.
    const float min_width =
        ImGui::CalcTextSize(title).x + ImGui::GetFrameHeight() + style.ItemInnerSpacing.x + 2.0F * style.FramePadding.x;
    constexpr float unlimited = std::numeric_limits<float>::max();
    ImGui::SetNextWindowSizeConstraints({min_width, 0.0F}, {unlimited, unlimited});
    const bool visible = ImGui::Begin(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    if (visible)
    {
        ImGui::BeginDisabled(tracking_pole_);
        draw_project(model);
        ImGui::EndDisabled();
        const auto &surface = model.current().definition;
        const auto selected_pole = selected_pole_;
        const auto columns = static_cast<std::size_t>(surface.u.pole_count);
        ImGui::Text("Surface degree   U %d   V %d", surface.u.degree, surface.v.degree);
        ImGui::Text("U [%.8g, %.8g]   V [%.8g, %.8g]", surface.u.knots.front().value, surface.u.knots.back().value,
                    surface.v.knots.front().value, surface.v.knots.back().value);
        ImGui::Text("Pole U %zu   V %zu", selected_pole % columns + 1, selected_pole / columns + 1);
        ImGui::BeginDisabled(tracking_pole_);
        auto &[x, y, z] = poles_.at(selected_pole);
        ImGui::PushID(static_cast<int>(selected_pole));
        ImGui::PushItemWidth(11.0F * ImGui::GetFontSize());
        ImGui::PushItemFlag(ImGuiItemFlags_LiveEditOnInputScalar, true);
        constexpr auto flags = ImGuiInputTextFlags_ParseEmptyRefVal;
        const bool changed_x = ImGui::InputDouble("X (mm)", &x, 0, 0, "%.8g", flags);
        const bool changed_y = ImGui::InputDouble("Y (mm)", &y, 0, 0, "%.8g", flags);
        const bool changed_z = ImGui::InputDouble("Z (mm)", &z, 0, 0, "%.8g", flags);
        const bool changed_weight = ImGui::InputDouble("Weight", &weights_.at(selected_pole), 0, 0, "%.8g", flags);
        ImGui::PopItemFlag();
        ImGui::PopItemWidth();
        ImGui::PopID();
        if (changed_x || changed_y || changed_z || changed_weight)
        {
            error_.clear();
        }

        bool dirty = has_edits(surface);
        ImGui::BeginDisabled(!dirty);
        if (ImGui::Button("Apply"))
        {
            const auto result = model.apply(poles_, weights_);
            if (result)
            {
                dirty = false;
                error_.clear();
            }
            else
            {
                error_ = result.error();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            reset_draft(model);
            dirty = false;
        }
        ImGui::EndDisabled();

        ImGui::BeginDisabled(dirty || !model.can_undo());
        if (ImGui::Button("Undo"))
        {
            model.undo();
            reset_draft(model);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(dirty || !model.can_redo());
        if (ImGui::Button("Redo"))
        {
            model.redo();
            reset_draft(model);
        }
        ImGui::EndDisabled();
        ImGui::BeginDisabled(dirty);
        draw_creation_dialog(model);
        ImGui::EndDisabled();
        ImGui::EndDisabled();
        const auto &point = surface.poles.at(selected_pole_);
        ImGui::Text("Applied (mm)   (%.8g, %.8g, %.8g)", point.x, point.y, point.z);
        ImGui::Text("Applied weight   %.8g", surface.weights.at(selected_pole_));
        if (!error_.empty())
        {
            ImGui::TextWrapped("%s", error_.c_str());
        }
        ImGui::BeginDisabled(tracking_pole_);
        auto sampling = model.sampling();
        if (draw_isoline_sampling_controls(sampling))
        {
            const auto result = model.resample(sampling);
            sampling_error_ = result ? std::string{} : result.error();
        }
        ImGui::EndDisabled();
        if (!sampling_error_.empty())
        {
            ImGui::TextWrapped("%s", sampling_error_.c_str());
        }
    }
    const auto *probe = update_probe(
        model.displayed(), visible && ImGui::CollapsingHeader("Surface probe", ImGuiTreeNodeFlags_DefaultOpen));
    ImGui::End();
    return probe;
}
