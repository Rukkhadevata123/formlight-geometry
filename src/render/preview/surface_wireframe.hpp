#pragma once

#include "geometry/surfaces/surface_isolines.hpp"
#include "render/viewport/viewport_projection.hpp"

struct PatchLayers
{
    bool control_net = true;
    bool constant_u = true;
    bool constant_v = true;
};

void draw_surface_wireframe(ImDrawList &draw_list, const occt_lab::render::ViewportProjection &project,
                            std::span<const occt_lab::geometry::Vec3> net, std::size_t u_count,
                            const occt_lab::geometry::PatchIsolines &isolines, const PatchLayers &layers,
                            std::size_t selected_pole);
