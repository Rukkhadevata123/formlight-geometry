module;

#include <imgui.h>

export module formlight_geometry.ui.surface.surface_grid_controls;

import std;
import formlight_geometry.geometry.surfaces.patch_grid;

export struct NurbsMeshControls
{
    formlight_geometry::geometry::PatchGrid grid;
    bool visible = false;
    bool requested = false;
    std::string report;
};

export [[nodiscard]] bool draw_grid_subdivisions(formlight_geometry::geometry::PatchGrid &grid)
{
    constexpr std::uint32_t minimum = 1;
    constexpr auto maximum = formlight_geometry::geometry::PatchGrid::max_segments;
    const bool u = ImGui::SliderScalar("U subdivisions", ImGuiDataType_U32, &grid.u_segments, &minimum, &maximum, "%u",
                                       ImGuiSliderFlags_AlwaysClamp);
    const bool v = ImGui::SliderScalar("V subdivisions", ImGuiDataType_U32, &grid.v_segments, &minimum, &maximum, "%u",
                                       ImGuiSliderFlags_AlwaysClamp);
    return u || v;
}
