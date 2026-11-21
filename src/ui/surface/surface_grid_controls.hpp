#pragma once

#include "geometry/surfaces/patch_grid.hpp"

#include <string>

[[nodiscard]] bool draw_grid_subdivisions(occt_lab::geometry::PatchGrid &grid);

struct NurbsComparisonControls
{
    occt_lab::geometry::PatchGrid grid;
    bool requested = false;
    std::string report;
};
