#include "box_model.hpp"
#include "brep_mesh.hpp"

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <NCollection_IndexedMap.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopLoc_Location.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <format>
#include <limits>
#include <print>
#include <stdexcept>
#include <string_view>

namespace
{
struct ExpectedShapeCount
{
    TopAbs_ShapeEnum type;
    std::string_view name;
    std::size_t count;
};

void check_topology(const TopoDS_Shape &box)
{
    constexpr std::array counts{
        ExpectedShapeCount{.type = TopAbs_SOLID, .name = "Solids", .count = 1},
        ExpectedShapeCount{.type = TopAbs_FACE, .name = "Faces", .count = 6},
        ExpectedShapeCount{.type = TopAbs_EDGE, .name = "Edges", .count = 12},
        ExpectedShapeCount{.type = TopAbs_VERTEX, .name = "Vertices", .count = 8},
    };

    for (const auto &[type, name, expected_count] : counts)
    {
        NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> shapes;
        TopExp::MapShapes(box, type, shapes);
        const auto count = shapes.Size();
        if (count != expected_count)
        {
            throw std::runtime_error(std::format("OCCT box {}: expected {}, got {}", name, expected_count, count));
        }
        std::println("  {}: {}", name, count);
    }
}

void check_mesh(const TopoDS_Shape &box, const double length, const double width, const double height)
{
    constexpr std::array translation{140.0, -30.0, 17.0};
    gp_Trsf transform;
    transform.SetTranslation(gp_Vec{translation[0], translation[1], translation[2]});
    const auto located_box = box.Moved(TopLoc_Location{transform});
    const auto mesh = triangulate_brep(located_box, gp_Pnt{}, 0.1);
    if (mesh.vertices.empty() || mesh.faces.size() != 6 || mesh.indices.size() != 36)
    {
        throw std::runtime_error("Box mesh should contain six faces and twelve triangles");
    }

    const std::array high{translation[0] + length, translation[1] + width, translation[2] + height};
    auto minimum = mesh.vertices.front().position;
    auto maximum = minimum;
    for (const auto &vertex : mesh.vertices)
    {
        for (std::size_t axis = 0; axis < minimum.size(); ++axis)
        {
            minimum[axis] = std::min(minimum[axis], vertex.position[axis]);
            maximum[axis] = std::max(maximum[axis], vertex.position[axis]);
        }
    }
    constexpr double coordinate_tolerance = 1e-5;
    for (std::size_t axis = 0; axis < minimum.size(); ++axis)
    {
        if (std::abs(minimum[axis] - translation[axis]) > coordinate_tolerance ||
            std::abs(maximum[axis] - high[axis]) > coordinate_tolerance)
        {
            throw std::runtime_error("Mesh bounds must include the source shape's Location");
        }
    }

    const gp_Pnt center{translation[0] + 0.5 * length, translation[1] + 0.5 * width, translation[2] + 0.5 * height};
    const auto position_at = [&mesh](const std::size_t index)
    {
        const auto &position = mesh.vertices.at(mesh.indices.at(index)).position;
        return gp_Pnt{position[0], position[1], position[2]};
    };
    for (std::size_t face_number = 0; face_number < mesh.faces.size(); ++face_number)
    {
        const auto [first, count] = mesh.faces[face_number];
        if (first != face_number * 6 || count != 6)
        {
            throw std::runtime_error("Expected two triangles in each box face's index range");
        }
        for (std::size_t triangle = first; triangle < first + count; triangle += 3)
        {
            const auto a = position_at(triangle);
            const auto normal = gp_Vec{a, position_at(triangle + 1)}.Crossed(gp_Vec{a, position_at(triangle + 2)});
            if (normal.Dot(gp_Vec{center, a}) <= 0.0)
            {
                throw std::runtime_error("Box triangle winding must face outwards");
            }
            const auto unit_normal = normal.Normalized();
            for (std::size_t corner = 0; corner < 3; ++corner)
            {
                if (const auto &out = mesh.vertices.at(mesh.indices.at(triangle + corner)).normal;
                    unit_normal.Dot(gp_Vec{out[0], out[1], out[2]}) < 1.0 - coordinate_tolerance)
                {
                    throw std::runtime_error("Box vertex normals must match the face normal");
                }
            }
        }
    }
    std::println("  Mesh: 6 faces, 12 triangles, translated bounds and outward normals verified");
}

void check_volume(const TopoDS_Shape &box, const double expected_volume)
{
    GProp_GProps properties;
    constexpr bool only_closed_shells = true;
    BRepGProp::VolumeProperties(box, properties, only_closed_shells);
    const double volume = properties.Mass();
    if (constexpr double relative_volume_tolerance = 1e-9;
        !std::isfinite(volume) || std::abs(volume - expected_volume) > expected_volume * relative_volume_tolerance)
    {
        throw std::runtime_error(
            std::format("OCCT box volume: expected {} mm^3, got {} mm^3", expected_volume, volume));
    }

    std::println("  Volume: {:.6f} mm^3 (expected {:.0f} mm^3)", volume, expected_volume);
}

void check_invalid_dimensions()
{
    constexpr std::array invalid_dimensions{
        BoxDimensions{.length = 0.0},
        BoxDimensions{.width = -1.0},
        BoxDimensions{.height = std::numeric_limits<double>::infinity()},
        BoxDimensions{.height = std::numeric_limits<double>::quiet_NaN()},
    };
    for (const auto &dimensions : invalid_dimensions)
    {
        try
        {
            (void)make_box(dimensions);
        }
        catch (const std::invalid_argument &)
        {
            continue;
        }
        throw std::runtime_error(std::format("Invalid box dimensions accepted: {:g} x {:g} x {:g}", dimensions.length,
                                             dimensions.width, dimensions.height));
    }
    std::println("  Invalid dimensions rejected");
}

void run_occt_check()
{
    constexpr BoxDimensions defaults;
    const auto [length, width, height] = defaults;
    std::println("OCCT box check ({} x {} x {} mm)", length, width, height);
    const auto box = make_box(defaults);
    check_topology(box.shape);
    check_volume(box.shape, length * width * height);
    check_mesh(box.shape, length, width, height);

    constexpr BoxDimensions edited{.length = 140.0, .width = 60.0, .height = 35.0};
    std::println("OCCT edited box check ({} x {} x {} mm)", edited.length, edited.width, edited.height);
    const auto resized = make_box(edited);
    if (resized.dimensions != edited)
    {
        throw std::runtime_error("The edited model must retain its dimensions");
    }
    check_volume(resized.shape, edited.length * edited.width * edited.height);
    check_mesh(resized.shape, edited.length, edited.width, edited.height);
    check_invalid_dimensions();
    std::println("OCCT box check passed");
}
} // namespace

int main()
{
    try
    {
        run_occt_check();
        return EXIT_SUCCESS;
    }
    catch (const std::exception &error)
    {
        std::println(stderr, "occt_box_check: {}", error.what());
        return EXIT_FAILURE;
    }
}
