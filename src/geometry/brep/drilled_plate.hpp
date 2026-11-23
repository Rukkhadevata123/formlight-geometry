#pragma once

#include "box_model.hpp"

#include <TopoDS_Solid.hxx>

// The plate occupies the same bounds as make_box; the centered hole runs along Z.
[[nodiscard]] TopoDS_Solid make_drilled_plate(const BoxDimensions &dimensions, double hole_radius);
