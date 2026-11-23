#include "plate_step.hpp"

#include "project_io.hpp"

#include <BRepAdaptor_Surface.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <STEPControl_Reader.hxx>
#include <STEPControl_Writer.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Iterator.hxx>

#include <cmath>
#include <fstream>
#include <format>
#include <optional>
#include <stdexcept>

namespace occt_lab::document
{
namespace
{
PlateMeasurements measure_plate(const TopoDS_Solid &plate)
{
    std::optional<double> wall_area;
    for (TopExp_Explorer faces{plate, TopAbs_FACE}; faces.More(); faces.Next())
    {
        const auto &face = TopoDS::Face(faces.Current());
        if (BRepAdaptor_Surface{face}.GetType() == GeomAbs_Cylinder)
        {
            if (wall_area)
            {
                throw std::runtime_error("Expected one cylindrical hole wall, found multiple cylindrical faces");
            }
            GProp_GProps surface;
            BRepGProp::SurfaceProperties(face, surface);
            wall_area = surface.Mass();
        }
    }
    if (!wall_area)
    {
        throw std::runtime_error("Plate has no cylindrical hole wall");
    }

    GProp_GProps volume;
    BRepGProp::VolumeProperties(plate, volume, true);
    return {.volume_mm3 = volume.Mass(), .hole_wall_area_mm2 = *wall_area};
}
} // namespace

void write_plate_step(const std::filesystem::path &path, const TopoDS_Solid &plate)
{
    STEPControl_Writer writer;
    if (writer.Transfer(plate, STEPControl_ManifoldSolidBrep) != IFSelect_RetDone)
    {
        throw std::runtime_error("Could not translate the plate to STEP");
    }
    const auto write_model = [&writer](std::ostream &out)
    {
        if (writer.WriteStream(out) != IFSelect_RetDone)
        {
            throw std::runtime_error("Could not write the STEP data");
        }
    };
    if (const auto saved = io::write_atomically(path, write_model); !saved)
    {
        throw std::runtime_error(saved.error());
    }
}

TopoDS_Solid read_step_solid(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        throw std::runtime_error("Could not open the STEP file");
    }
    STEPControl_Reader reader;
    const auto name = path.u8string();
    if (reader.ReadStream(std::string(name.begin(), name.end()).c_str(), in) != IFSelect_RetDone)
    {
        throw std::runtime_error("Could not parse the STEP file");
    }
    reader.SetSystemLengthUnit(1.0); // Millimeters.
    if (reader.NbRootsForTransfer() != 1 || !reader.TransferRoot(1))
    {
        throw std::runtime_error("Expected one transferable STEP root");
    }

    auto shape = reader.OneShape();
    while (!shape.IsNull() && shape.ShapeType() == TopAbs_COMPOUND)
    {
        TopoDS_Iterator children(shape);
        if (!children.More())
        {
            throw std::runtime_error("Empty STEP compound");
        }
        const auto child = children.Value();
        children.Next();
        if (children.More())
        {
            throw std::runtime_error("Expected a single STEP solid");
        }
        shape = child;
    }
    if (shape.IsNull() || shape.ShapeType() != TopAbs_SOLID || !BRepCheck_Analyzer{shape}.IsValid())
    {
        throw std::runtime_error("STEP root did not produce one valid solid");
    }
    return TopoDS::Solid(shape);
}

PlateMeasurements verify_plate_step(const TopoDS_Solid &original, const TopoDS_Solid &imported)
{
    const auto expected = measure_plate(original);
    const auto actual = measure_plate(imported);
    const auto matches = [](double before, double after) { return std::abs(after - before) <= 1e-8 * before; };
    if (!matches(expected.volume_mm3, actual.volume_mm3) ||
        !matches(expected.hole_wall_area_mm2, actual.hole_wall_area_mm2))
    {
        throw std::runtime_error(std::format(
            "STEP mismatch: volume {} vs {} mm^3, wall area {} vs {} mm^2 (file vs plate)", actual.volume_mm3,
            expected.volume_mm3, actual.hole_wall_area_mm2, expected.hole_wall_area_mm2));
    }
    return actual;
}
} // namespace occt_lab::document
