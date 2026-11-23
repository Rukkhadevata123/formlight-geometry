#include "drilled_plate.hpp"

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <Precision.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Ax2.hxx>

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>

TopoDS_Solid make_drilled_plate(const BoxDimensions &dimensions, const double hole_radius)
{
    const auto plate = make_box(dimensions);
    const auto [length, width, thickness] = dimensions;
    const double clearance = 0.5 * std::min(length, width) - hole_radius;
    if (!std::isfinite(hole_radius) || hole_radius <= Precision::Confusion() || clearance <= Precision::Confusion())
    {
        throw std::invalid_argument("Hole radius must be positive and leave material around the hole");
    }

    // Extend the cutter past both faces so neither cylinder cap coincides with the plate.
    const gp_Ax2 axis{gp_Pnt{0.5 * length, 0.5 * width, -thickness}, gp::DZ()};
    BRepPrimAPI_MakeCylinder cutter{axis, hole_radius, 3.0 * thickness};
    BRepAlgoAPI_Cut cut;
    cut.SetArguments({plate.shape});
    cut.SetTools({cutter.Shape()});
    cut.Build();
    if (!cut.IsDone() || cut.HasErrors())
    {
        std::ostringstream errors;
        cut.DumpErrors(errors);
        throw std::runtime_error("OCCT plate cut failed: " + errors.str());
    }

    const auto &result = cut.Shape();
    TopExp_Explorer solids{result, TopAbs_SOLID};
    if (!solids.More())
    {
        throw std::runtime_error("Plate cut produced no solid");
    }
    const auto solid = TopoDS::Solid(solids.Current());
    solids.Next();
    if (solids.More() || !BRepCheck_Analyzer{result}.IsValid())
    {
        throw std::runtime_error("Plate cut did not produce one valid solid");
    }
    return solid;
}
