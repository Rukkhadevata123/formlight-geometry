module;

#include <imgui.h>

module formlight_geometry.ui.surface.nurbs_pole_inspector;

import std;
import formlight_geometry.application.editable_nurbs_surface;
import formlight_geometry.geometry.surfaces.bernstein_reference;
import formlight_geometry.geometry.surfaces.bezier_patch;
import formlight_geometry.geometry.surfaces.bspline_surface;
import formlight_geometry.ui.surface.surface_sample_readout;

namespace
{
bool supports_slider(const double low, const double high)
{
    // ImGui's double slider requires both endpoints to lie within half the numeric range.
    constexpr double limit = std::numeric_limits<double>::max() / 2.0;
    return low >= -limit && high <= limit;
}

bool parameter_control(const char *label, double &value, const double low, const double high)
{
    double edited = value;
    if (supports_slider(low, high))
    {
        ImGui::SliderScalar(label, ImGuiDataType_Double, &edited, &low, &high, "%.10g",
                            ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoRoundToFormat);
    }
    else
    {
        ImGui::InputDouble(label, &edited, 0, 0, "%.17g");
    }
    if (!std::isfinite(edited) || edited == value)
    {
        return false;
    }
    value = std::clamp(edited, low, high);
    return true;
}
} // namespace

const formlight_geometry::geometry::SurfaceProbe *NurbsPoleInspector::update_probe(const EditableNurbsSurface::Snapshot &snapshot,
                                                                         const bool show_controls)
{
    const bool surface_changed = snapshot.surface != probed_surface_;
    const auto bounds = snapshot.surface->domain();
    const bool center_requested = show_controls && ImGui::Button("Center UV probe");
    if (surface_changed || center_requested)
    {
        const auto old = probed_surface_ ? probed_surface_->domain() : formlight_geometry::geometry::SurfaceDomain{};
        if (center_requested || old.u_min != bounds.u_min || old.u_max != bounds.u_max)
        {
            probe_u_ = std::midpoint(bounds.u_min, bounds.u_max);
        }
        if (center_requested || old.v_min != bounds.v_min || old.v_max != bounds.v_max)
        {
            probe_v_ = std::midpoint(bounds.v_min, bounds.v_max);
        }
    }
    bool parameters_changed = false;
    if (show_controls)
    {
        ImGui::PushItemWidth(11.0F * ImGui::GetFontSize());
        const bool u_changed = parameter_control("u", probe_u_, bounds.u_min, bounds.u_max);
        const bool v_changed = parameter_control("v", probe_v_, bounds.v_min, bounds.v_max);
        parameters_changed = u_changed || v_changed;
        ImGui::PopItemWidth();
        if (supports_slider(bounds.u_min, bounds.u_max) || supports_slider(bounds.v_min, bounds.v_max))
        {
            ImGui::TextUnformatted(parameter_slider_hint);
        }
    }
    if (surface_changed || center_requested || parameters_changed)
    {
        probe_ = snapshot.surface->probe(probe_u_, probe_v_);
        bernstein_sample_ = evaluate_bernstein_reference(snapshot.definition, probe_u_, probe_v_);
        probed_surface_ = snapshot.surface;
    }
    if (show_controls)
    {
        ImGui::SeparatorText("OCCT reference");
        if (!probe_)
        {
            ImGui::TextWrapped("%s", probe_.error().c_str());
        }
        else if (probe_->normal)
        {
            draw_surface_sample({probe_->position, *probe_->normal});
        }
        else
        {
            const auto &[x, y, z] = probe_->position;
            ImGui::Text("Position (mm)   (%.8g, %.8g, %.8g)", x, y, z);
            ImGui::TextWrapped("Normal unavailable: %s", probe_->normal.error().c_str());
        }
        draw_bernstein_sample(bernstein_sample_);
        if (bernstein_sample_ && probe_ && probe_->normal)
        {
            draw_surface_difference(*bernstein_sample_, {probe_->position, *probe_->normal});
        }
    }
    return probe_ ? &*probe_ : nullptr;
}
