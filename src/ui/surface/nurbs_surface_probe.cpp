#include "nurbs_pole_inspector.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace
{
bool parameter_control(const char *label, double &value, const double low, const double high)
{
    // ImGui's double slider requires both endpoints to lie within half the numeric range.
    constexpr double limit = std::numeric_limits<double>::max() / 2.0;
    double edited = value;
    if (low >= -limit && high <= limit)
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

const occt_lab::geometry::SurfaceProbe *NurbsPoleInspector::update_probe(const EditableNurbsSurface::Snapshot &snapshot,
                                                                         const bool show_controls)
{
    const bool surface_changed = snapshot.surface != probed_surface_;
    const auto bounds = snapshot.surface->domain();
    if (surface_changed)
    {
        const auto old = probed_surface_ ? probed_surface_->domain() : occt_lab::geometry::SurfaceDomain{};
        if (old.u_min != bounds.u_min || old.u_max != bounds.u_max)
        {
            probe_u_ = std::midpoint(bounds.u_min, bounds.u_max);
        }
        if (old.v_min != bounds.v_min || old.v_max != bounds.v_max)
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
    }
    if (surface_changed || parameters_changed)
    {
        probe_ = snapshot.surface->probe(probe_u_, probe_v_);
        probed_surface_ = snapshot.surface;
    }
    if (!probe_)
    {
        if (show_controls)
        {
            ImGui::TextWrapped("%s", probe_.error().c_str());
        }
        return nullptr;
    }
    if (show_controls)
    {
        const auto &[x, y, z] = probe_->position;
        ImGui::Text("Position (mm)   (%.8g, %.8g, %.8g)", x, y, z);
        if (const auto &normal = probe_->normal)
        {
            ImGui::Text("Unit normal   (%.8g, %.8g, %.8g)", normal->x, normal->y, normal->z);
        }
        else
        {
            ImGui::TextWrapped("Normal unavailable: %s", normal.error().c_str());
        }
    }
    return &*probe_;
}
