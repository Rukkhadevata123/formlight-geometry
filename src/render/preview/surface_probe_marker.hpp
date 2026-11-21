#pragma once

#include "render/viewport/viewport_projection.hpp"

#include <optional>

void draw_surface_probe_marker(ImDrawList &draw_list, const occt_lab::render::ViewportProjection &project,
                               occt_lab::geometry::Vec3 point, const std::optional<occt_lab::geometry::Vec3> &normal,
                               double arrow_length);
