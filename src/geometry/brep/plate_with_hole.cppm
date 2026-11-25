module;

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <Precision.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Pln.hxx>
#include <gp_Vec.hxx>

export module formlight_geometry.geometry.brep.plate_with_hole;

import std;
import formlight_geometry.geometry.brep.box_model;

namespace
{
void validate_hole_radius(const BoxDimensions &dimensions, const double hole_radius)
{
    const double clearance = 0.5 * std::min(dimensions.length, dimensions.width) - hole_radius;
    if (!std::isfinite(hole_radius) || hole_radius <= Precision::Confusion() || clearance <= Precision::Confusion())
    {
        throw std::invalid_argument("Hole radius must be positive and leave material around the hole");
    }
}
} // namespace

// The plate occupies the same bounds as make_box; the centered hole runs along Z.
export [[nodiscard]] TopoDS_Solid cut_plate_with_hole(const BoxDimensions &dimensions, const double hole_radius)
{
    const auto plate = make_box(dimensions);
    validate_hole_radius(dimensions, hole_radius);
    const auto [length, width, thickness] = dimensions;

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

export [[nodiscard]] TopoDS_Solid extrude_plate_with_hole(const BoxDimensions &dimensions, const double hole_radius)
{
    dimensions.validate();
    validate_hole_radius(dimensions, hole_radius);
    const auto [length, width, thickness] = dimensions;

    BRepBuilderAPI_MakeFace profile{gp_Pln{gp::XOY()}, 0.0, length, 0.0, width};
    const gp_Ax2 circle_axis{gp_Pnt{0.5 * length, 0.5 * width, 0.0}, gp::DZ()};
    BRepBuilderAPI_MakeEdge circle{gp_Circ{circle_axis, hole_radius}};
    BRepBuilderAPI_MakeWire hole{circle.Edge()};
    // The bounded plane supplies the outer wire. Its inner wire must have the opposite winding.
    profile.Add(TopoDS::Wire(hole.Wire().Reversed()));
    if (!profile.IsDone() || !BRepCheck_Analyzer{profile.Face()}.IsValid())
    {
        throw std::runtime_error("Invalid rectangular extrusion profile with circular hole");
    }

    BRepPrimAPI_MakePrism prism{profile.Face(), gp_Vec{0.0, 0.0, thickness}};
    if (!prism.IsDone() || prism.Shape().ShapeType() != TopAbs_SOLID || !BRepCheck_Analyzer{prism.Shape()}.IsValid())
    {
        throw std::runtime_error("Plate extrusion did not produce a valid solid");
    }
    return TopoDS::Solid(prism.Shape());
}
