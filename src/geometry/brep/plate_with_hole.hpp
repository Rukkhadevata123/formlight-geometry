#pragma once

#include "box_model.hpp"

#include <TopoDS_Solid.hxx>

// The plate occupies the same bounds as make_box; the centered hole runs along Z.
[[nodiscard]] TopoDS_Solid cut_plate_with_hole(const BoxDimensions &dimensions, double hole_radius);
[[nodiscard]] TopoDS_Solid extrude_plate_with_hole(const BoxDimensions &dimensions, double hole_radius);
