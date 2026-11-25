module;

#include <imgui.h>

module formlight_geometry.ui.surface.nurbs_pole_inspector;

import std;
import formlight_geometry.application.bezier_patch_presets;
import formlight_geometry.application.editable_nurbs_surface;
import formlight_geometry.application.nurbs_surface_creation;
import formlight_geometry.geometry.bspline_knot;
import formlight_geometry.ui.clamped_knot_editor;

namespace
{
using KnotList = std::vector<formlight_geometry::geometry::BSplineKnot>;

bool draw_creation_axis(const char *poles_label, const char *degree_label, const char *knots_label, int &poles,
                        std::optional<int> &degree, std::optional<KnotList> &knots)
{
    using Params = formlight_geometry::SurfaceCreationParameters;
    constexpr auto flags = ImGuiSliderFlags_AlwaysClamp;
    const bool poles_changed = ImGui::SliderInt(poles_label, &poles, Params::min_poles, Params::max_poles, "%d", flags);
    const int limit = Params::degree_limit(poles);
    if (degree && *degree > limit)
    {
        degree = limit;
    }
    int shown = degree.value_or(std::min(Params::preferred_degree, limit));
    ImGui::BeginDisabled(limit == 1);
    const bool degree_changed = ImGui::SliderInt(degree_label, &shown, 1, limit, "%d", flags);
    ImGui::EndDisabled();
    if (degree_changed)
    {
        degree = shown;
    }
    const bool knots_changed = draw_custom_knots(knots_label, poles, shown, knots);
    return poles_changed || degree_changed || knots_changed;
}

bool draw_axes(formlight_geometry::SurfaceCreationParameters &p)
{
    const bool u_changed =
        draw_creation_axis("U poles", "U degree", "Custom U knots", p.u_poles, p.u_degree, p.u_knots);
    const bool v_changed =
        draw_creation_axis("V poles", "V degree", "Custom V knots", p.v_poles, p.v_degree, p.v_knots);
    return u_changed || v_changed;
}

bool draw_plane_parameters(formlight_geometry::SurfaceCreationParameters &p)
{
    const bool axes_changed = draw_axes(p);
    ImGui::PushItemFlag(ImGuiItemFlags_LiveEditOnInputScalar, true);
    const bool width_changed = ImGui::InputDouble("Width (X, mm)", &p.width_mm, 0, 0, "%.10g");
    const bool height_changed = ImGui::InputDouble("Height (Y, mm)", &p.height_mm, 0, 0, "%.10g");
    ImGui::PopItemFlag();
    return axes_changed || width_changed || height_changed;
}

enum CreationPreset
{
    custom_plane,
    from_control_net,
    interpolate_grid,
    approximate_grid,
    bezier_plane,
    raised_bezier
};

auto create_from_grid(const int preset, const formlight_geometry::SurfaceCreationParameters &parameters,
                      const formlight_geometry::SurfacePoleRows &grid, const double tolerance_mm)
    -> std::expected<EditableNurbsSurface::Definition, std::string>
{
    switch (preset)
    {
    case from_control_net:
        return formlight_geometry::make_control_net_surface(parameters, grid);
    case interpolate_grid:
        return formlight_geometry::interpolate_surface_grid(grid);
    case approximate_grid:
        return formlight_geometry::approximate_surface_grid(grid, tolerance_mm);
    default:
        return std::unexpected("Unknown point-grid preset.");
    }
}

auto create_definition(const int preset, const formlight_geometry::SurfaceCreationParameters &parameters)
    -> std::expected<EditableNurbsSurface::Definition, std::string>
{
    switch (preset)
    {
    case custom_plane:
        return formlight_geometry::make_planar_nurbs_surface(parameters);
    case bezier_plane:
        return formlight_geometry::make_bezier_nurbs_surface(formlight_geometry::planar_bezier_net());
    case raised_bezier:
        return formlight_geometry::make_bezier_nurbs_surface(formlight_geometry::raised_bezier_net());
    default:
        return std::unexpected("Unknown surface preset.");
    }
}
} // namespace

void NurbsPoleInspector::cancel_creation(EditableNurbsSurface &model)
{
    if (net_preview_)
    {
        model.discard_preview();
        net_preview_ = false;
    }
    picked_net_.reset();
    resume_net_ = false;
}

void NurbsPoleInspector::draw_creation_dialog(EditableNurbsSurface &model)
{
    constexpr char title[] = "New surface";
    const bool reopen = std::exchange(resume_net_, false);
    if (ImGui::Button("New surface...") && !reopen)
    {
        creation_parameters_ = {};
        net_parameters_ = {.v_poles = 3};
        creation_net_.clear();
        fit_tolerance_mm_ = 0.1;
        net_selected_ = 0;
        net_frame_.reset();
        creation_preset_ = custom_plane;
        creation_error_.clear();
        ImGui::OpenPopup(title);
    }
    if (reopen)
    {
        ImGui::OpenPopup(title);
    }
    if (!ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        cancel_creation(model);
        return;
    }
    ImGui::PushItemWidth(12.0F * ImGui::GetFontSize());
    const bool preset_changed = ImGui::Combo("Preset", &creation_preset_,
                                             "Custom plane\0From control net\0Interpolate through point grid\0"
                                             "Approximate point grid\0Bezier plane\0Raised Bezier patch\0");
    const bool data_points = creation_preset_ == interpolate_grid || creation_preset_ == approximate_grid;
    const bool uses_grid = creation_preset_ == from_control_net || data_points;
    bool parameters_changed = creation_preset_ == custom_plane && draw_plane_parameters(creation_parameters_);
    if (uses_grid)
    {
        if (data_points)
        {
            using Params = formlight_geometry::SurfaceCreationParameters;
            const bool u_changed = ImGui::SliderInt("U points", &net_parameters_.u_poles, Params::min_poles,
                                                    Params::max_poles, "%d", ImGuiSliderFlags_AlwaysClamp);
            const bool v_changed = ImGui::SliderInt("V points", &net_parameters_.v_poles, Params::min_poles,
                                                    Params::max_poles, "%d", ImGuiSliderFlags_AlwaysClamp);
            parameters_changed = u_changed || v_changed;
            if (creation_preset_ == approximate_grid)
            {
                ImGui::PushItemFlag(ImGuiItemFlags_LiveEditOnInputScalar, true);
                const bool tolerance_changed = ImGui::InputDouble("Tolerance (mm)", &fit_tolerance_mm_, 0, 0, "%.8g");
                ImGui::PopItemFlag();
                parameters_changed |= tolerance_changed;
            }
        }
        else
        {
            parameters_changed = draw_axes(net_parameters_);
        }
        const auto columns = net_parameters_.u_poles, rows = net_parameters_.v_poles;
        if (creation_net_.size() != static_cast<std::size_t>(rows) ||
            creation_net_.front().size() != static_cast<std::size_t>(columns))
        {
            formlight_geometry::resize_surface_net(creation_net_, columns, rows);
            net_selected_ = 0;
        }
        if (draw_net_table(!data_points))
        {
            parameters_changed = true;
        }
        if (ImGui::Button(data_points ? "Pick data points on XY plane..." : "Pick net on XY plane..."))
        {
            const auto frame = model.displayed().framing;
            cancel_creation(model);
            picked_net_ = NetPicking{.framing = frame, .positions = {}};
            creation_error_.clear();
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::PopItemWidth();
    if (!picking_net() && (preset_changed || parameters_changed || reopen))
    {
        creation_error_.clear();
        if (uses_grid)
        {
            auto definition = create_from_grid(creation_preset_, net_parameters_, creation_net_, fit_tolerance_mm_);
            auto preview = std::move(definition).and_then([&](EditableNurbsSurface::Definition &&def)
                                                         { return model.preview_replacement(std::move(def), net_frame_); });
            net_preview_ = preview.has_value();
            if (!preview)
            {
                model.discard_preview();
                creation_error_ = preview.error();
            }
        }
        else
        {
            cancel_creation(model);
        }
    }
    if (data_points && net_preview_)
    {
        const auto &preview = model.displayed();
        draw_max_grid_deviation(preview.max_grid_deviation.value());
        ImGui::Text("Control net: %d x %d", preview.definition.u.pole_count, preview.definition.v.pole_count);
    }
    ImGui::BeginDisabled(uses_grid && !net_preview_);
    if (ImGui::Button("Create"))
    {
        std::expected<void, std::string> result;
        if (uses_grid)
        {
            model.commit_preview();
            net_preview_ = false;
        }
        else
        {
            result = create_definition(creation_preset_, creation_parameters_)
                         .and_then([&](EditableNurbsSurface::Definition &&definition)
                                   { return model.replace(std::move(definition)); });
        }
        if (result)
        {
            selected_pole_ = 0;
            reset_draft(model);
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
        cancel_creation(model);
        ImGui::CloseCurrentPopup();
    }
    if (!creation_error_.empty())
    {
        ImGui::TextUnformatted(creation_error_.c_str());
    }
    ImGui::EndPopup();
}
