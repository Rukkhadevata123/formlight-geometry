#include "geometry/brep/brep_face_properties.hpp"
#include "geometry/brep/brep_mesh.hpp"
#include "geometry/brep/drilled_plate.hpp"
#include "render/viewport/brep_face_picking.hpp"
#include "render/viewport/clip_transform.hpp"

#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <gp_Pnt.hxx>

#include <cmath>
#include <cstdlib>
#include <exception>
#include <format>
#include <numbers>
#include <print>
#include <stdexcept>

namespace
{
constexpr BoxDimensions dimensions{.length = 100.0, .width = 80.0, .height = 20.0};
constexpr double radius = 20.0;

void check_solid(const TopoDS_Solid &plate)
{
    if (plate.IsNull() || !BRepCheck_Analyzer{plate}.IsValid())
    {
        throw std::runtime_error("The drilled plate must be a valid OCCT solid");
    }

    GProp_GProps properties;
    BRepGProp::VolumeProperties(plate, properties, true);
    const double expected =
        (dimensions.length * dimensions.width - std::numbers::pi * radius * radius) * dimensions.height;
    if (!(std::abs(properties.Mass() - expected) <= expected * 1e-9))
    {
        throw std::runtime_error(std::format("Expected plate volume {} mm^3, got {}", expected, properties.Mass()));
    }
}

void check_face_picking(const TopoDS_Solid &plate)
{
    const gp_Pnt center{0.5 * dimensions.length, 0.5 * dimensions.width, 0.5 * dimensions.height};
    const auto mesh = triangulate_brep(plate, center, 0.1);
    constexpr ClipTransform top{
        .x = {1.F / 64, 0, 0, 0},
        .y = {0, -1.F / 64, 0, 0},
        .z = {0, 0, -1.F / 128, 0.5F},
        .w = {0, 0, 0, 1},
    };
    if (pick_brep_face(mesh, top, 0, 0))
    {
        throw std::runtime_error("A top-down pick through the hole must miss");
    }
    const auto top_face = pick_brep_face(mesh, top, 0.6, 0);
    if (!top_face || inspect_brep_face(mesh, *top_face).surface_type != GeomAbs_Plane)
    {
        throw std::runtime_error("A top-down pick outside the hole must hit the plate");
    }

    // Tilt the rays towards +X as Z decreases, so they enter the hole and hit its far wall.
    auto oblique = top;
    oblique.x[2] = oblique.x[0];
    const auto wall = pick_brep_face(mesh, oblique, 0.22, 0.1);
    if (!wall || pick_brep_face(mesh, oblique, 0.22, -0.1) != wall)
    {
        throw std::runtime_error("Picks on different wall triangles must select the same face");
    }
    const auto properties = inspect_brep_face(mesh, *wall);
    const double wall_area = 2.0 * std::numbers::pi * radius * dimensions.height;
    if (properties.surface_type != GeomAbs_Cylinder || !(std::abs(properties.area_mm2 - wall_area) <= wall_area * 1e-9))
    {
        throw std::runtime_error("The selected face must cover the whole cylindrical hole wall");
    }
}
} // namespace

int main()
{
    try
    {
        const auto plate = make_drilled_plate(dimensions, radius);
        check_solid(plate);
        check_face_picking(plate);
        std::println("Drilled plate validity, volume and hole picking verified");
        return EXIT_SUCCESS;
    }
    catch (const std::exception &error)
    {
        std::println(stderr, "occt_drilled_plate_check: {}", error.what());
        return EXIT_FAILURE;
    }
}
