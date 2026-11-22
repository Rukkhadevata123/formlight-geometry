#include "geometry/brep/brep_mesh.hpp"
#include "render/viewport/brep_face_picking.hpp"
#include "render/viewport/clip_transform.hpp"

#include <BRepAdaptor_Surface.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <gp_Pnt.hxx>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <print>
#include <span>
#include <stdexcept>

namespace
{
void check_pick_depth()
{
    constexpr ClipTransform view{.x = {1, 0, 0, 0}, .y = {0, -1, 0, 0}, .z = {0, 0, -1, 1}, .w = {0, 0, 0, 1}};
    BRepMeshData mesh;
    const auto add_triangle = [&](const float z, const bool reversed)
    {
        const auto base = static_cast<uint32_t>(mesh.vertices.size());
        const auto first = static_cast<uint32_t>(mesh.indices.size());
        mesh.vertices.insert(mesh.vertices.end(), {{.position = {-1, -1, z}, .normal = {}},
                                                   {.position = {1, -1, z}, .normal = {}},
                                                   {.position = {0, 1, z}, .normal = {}}});
        mesh.indices.insert(mesh.indices.end(), {base, base + (reversed ? 2U : 1U), base + (reversed ? 1U : 2U)});
        mesh.faces.push_back({.first_index = first, .index_count = 3, .face = {}});
    };
    add_triangle(2, false);   // Before the near plane.
    add_triangle(-1, false);  // Beyond the far plane.
    add_triangle(0.9F, true); // Back-facing.
    if (pick_brep_face(mesh, view, 0, 0))
    {
        throw std::runtime_error("Picking must reject clipped and culled triangles");
    }
    add_triangle(0.2F, false);
    add_triangle(0.7F, false);
    if (pick_brep_face(mesh, view, 0, 0) != 4)
    {
        throw std::runtime_error("Overlapping front-facing faces must select the nearest hit");
    }
    auto singular = view;
    singular.y = singular.x;
    if (pick_brep_face(mesh, singular, 0, 0))
    {
        throw std::runtime_error("A singular clip transform must not select a face");
    }
}

void check_face_picking(const BRepMeshData &mesh)
{
    constexpr ClipTransform top{.x = {1.F / 64, 0, 0, 0}, .y = {0, -1.F / 64, 0, 0},
                                .z = {0, 0, -1.F / 128, 0.5F}, .w = {0, 0, 0, 1}};
    const auto cap = pick_brep_face(mesh, top, 0.1, -0.08);
    if (!cap || pick_brep_face(mesh, top, 0.95, 0.95))
    {
        throw std::runtime_error("Top view must pick the cap and miss empty space");
    }
    if (const BRepAdaptor_Surface plane{mesh.faces[*cap].face};
        plane.GetType() != GeomAbs_Plane || !(std::abs(plane.Plane().Location().Z() - 60) <= 1e-10))
    {
        throw std::runtime_error("Top view must resolve to the upper OCCT face");
    }
    // Perspective from +X: w = 100-X. Points on opposite sides of the image hit separate side triangles.
    constexpr ClipTransform side{.x = {0, 1, 0, 0}, .y = {0, 0, -1, 0}, .z = {-1.5F, 0, 0, 75}, .w = {-1, 0, 0, 100}};
    const auto left = pick_brep_face(mesh, side, -0.2, 0.1);
    if (const auto right = pick_brep_face(mesh, side, 0.2, -0.1);
        !left || left != right || BRepAdaptor_Surface{mesh.faces[*left].face}.GetType() != GeomAbs_Cylinder)
    {
        throw std::runtime_error("Both triangles must resolve to the same cylindrical OCCT face");
    }
}

void check_cylinder_normals()
{
    constexpr double radius = 40.0;
    constexpr double half_height = 30.0;
    constexpr double tolerance = 1e-5;
    BRepPrimAPI_MakeCylinder cylinder{radius, 2.0 * half_height};
    const auto mesh = triangulate_brep(cylinder.Shape(), gp_Pnt{0.0, 0.0, half_height}, 0.1);
    check_face_picking(mesh);
    if (mesh.faces.size() != 3)
    {
        throw std::runtime_error("Expected three face ranges for the cylinder");
    }

    int side_faces = 0;
    for (const auto &face : mesh.faces)
    {
        const auto first_index = face.first_index, index_count = face.index_count;
        if (first_index > mesh.indices.size() || index_count == 0 || index_count > mesh.indices.size() - first_index)
        {
            throw std::runtime_error("Cylinder face has an invalid index range");
        }
        const auto indices = std::span{mesh.indices}.subspan(first_index, index_count);
        const double first_z = mesh.vertices.at(indices.front()).position[2];
        const bool is_cap =
            std::ranges::all_of(indices, [&mesh, first_z](const auto index)
                                { return std::abs(mesh.vertices.at(index).position[2] - first_z) < tolerance; });
        if (!is_cap)
        {
            ++side_faces;
        }

        // The radial check also covers the duplicated vertices along the periodic seam.
        for (const auto index : indices)
        {
            const auto &[position, normal] = mesh.vertices.at(index);
            const auto expected = is_cap ? std::array{0.0, 0.0, std::copysign(1.0, first_z)}
                                         : std::array{position[0] / radius, position[1] / radius, 0.0};
            if (const double error =
                    std::hypot(normal[0] - expected[0], normal[1] - expected[1], normal[2] - expected[2]);
                !std::isfinite(error) || error > tolerance)
            {
                throw std::runtime_error(is_cap ? "Cylinder cap normal is not axial"
                                                : "Cylinder side normal is not radial");
            }
        }
    }
    if (side_faces != 1)
    {
        throw std::runtime_error("Expected one curved side and two flat caps for the cylinder");
    }
}
} // namespace

int main()
{
    try
    {
        check_cylinder_normals();
        check_pick_depth();
        std::println("OCCT cylinder normals and face picking verified");
        return EXIT_SUCCESS;
    }
    catch (const std::exception &error)
    {
        std::println(stderr, "occt_cylinder_check: {}", error.what());
        return EXIT_FAILURE;
    }
}
