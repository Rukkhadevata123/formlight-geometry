#include <GeomAbs_SurfaceType.hxx>
#include <TopoDS_Solid.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <cstdio>
#include <cstdlib>
#include <gp_Pnt.hxx>

import std;
import occt_lab.document.plate_step;
import occt_lab.geometry.brep.box_model;
import occt_lab.geometry.brep.brep_face_properties;
import occt_lab.geometry.brep.brep_mesh;
import occt_lab.geometry.brep.plate_with_hole;
import occt_lab.render.viewport.brep_face_picking;
import occt_lab.render.viewport.clip_transform;
import occt_lab.test.support.project_test_directory;

namespace
{
constexpr BoxDimensions dimensions{.length = 100.0, .width = 80.0, .height = 20.0};
constexpr double radius = 20.0;

double check_solid(const TopoDS_Solid &plate)
{
    if (plate.IsNull() || !BRepCheck_Analyzer{plate}.IsValid())
    {
        throw std::runtime_error("The plate with a hole must be a valid OCCT solid");
    }

    GProp_GProps properties;
    BRepGProp::VolumeProperties(plate, properties, true);
    const double expected =
        (dimensions.length * dimensions.width - std::numbers::pi * radius * radius) * dimensions.height;
    if (!(std::abs(properties.Mass() - expected) <= expected * 1e-9))
    {
        throw std::runtime_error(std::format("Expected plate volume {} mm^3, got {}", expected, properties.Mass()));
    }
    const gp_Pnt expected_center{0.5 * dimensions.length, 0.5 * dimensions.width, 0.5 * dimensions.height};
    if (!(properties.CentreOfMass().Distance(expected_center) <= 1e-8))
    {
        throw std::runtime_error("Plate center of mass must be halfway between the base and top faces");
    }
    return properties.Mass();
}

double check_face_picking(const TopoDS_Solid &plate)
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
    return properties.area_mm2;
}

void check_plate(const char *method, const TopoDS_Solid &plate, const std::filesystem::path &file)
{
    const double volume = check_solid(plate);
    const double wall_area = check_face_picking(plate);
    std::println("{}: volume {:.6f} mm^3, selected hole wall {:.6f} mm^2", method, volume, wall_area);

    occt_lab::document::write_plate_step(file, plate);
    const auto imported = occt_lab::document::read_step_solid(file);
    const auto measures = occt_lab::document::verify_plate_step(plate, imported);
    check_solid(imported);
    check_face_picking(imported);
    std::println("{} STEP: volume {:.6f} mm^3, cylindrical wall {:.6f} mm^2", method, measures.volume_mm3,
                 measures.hole_wall_area_mm2);
}

void check_step_failures(const std::filesystem::path &file)
{
    const auto imported = occt_lab::document::read_step_solid(file);
    const auto smaller_hole = cut_plate_with_hole(dimensions, 0.5 * radius);
    bool mismatch_detected = false;
    try
    {
        (void)occt_lab::document::verify_plate_step(smaller_hole, imported);
    }
    catch (const std::runtime_error &)
    {
        mismatch_detected = true;
    }
    if (!mismatch_detected)
    {
        throw std::runtime_error("STEP verification accepted a different hole radius");
    }

    std::ofstream(file) << "invalid STEP";
    try
    {
        (void)occt_lab::document::read_step_solid(file);
    }
    catch (const std::runtime_error &)
    {
        return;
    }
    throw std::runtime_error("STEP reader accepted a malformed file");
}
} // namespace

int main()
{
    try
    {
        const ProjectTestDirectory directory;
        const auto file = directory.path / u8"带孔板.step";
        check_plate("Boolean cut", cut_plate_with_hole(dimensions, radius), file);
        check_plate("Profile extrusion", extrude_plate_with_hole(dimensions, radius), file);
        check_step_failures(file);
        return EXIT_SUCCESS;
    }
    catch (const std::exception &error)
    {
        std::println(stderr, "occt_plate_with_hole_check: {}", error.what());
        return EXIT_FAILURE;
    }
}
