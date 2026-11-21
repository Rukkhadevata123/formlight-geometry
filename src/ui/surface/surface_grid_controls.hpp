#pragma once

#include "geometry/surfaces/patch_grid.hpp"

#include <string>

[[nodiscard]] bool draw_grid_subdivisions(occt_lab::geometry::PatchGrid &grid);

struct NurbsMeshControls
{
    occt_lab::geometry::PatchGrid grid;
    bool visible = false;
    bool requested = false;
    std::string report;
};
