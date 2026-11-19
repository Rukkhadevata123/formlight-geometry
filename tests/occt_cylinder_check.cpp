#include "geometry/brep_mesh.hpp"

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
void check_cylinder_normals()
{
    constexpr double radius = 40.0;
    constexpr double half_height = 30.0;
    constexpr double tolerance = 1e-5;
    BRepPrimAPI_MakeCylinder cylinder{radius, 2.0 * half_height};
    const auto mesh = triangulate_brep(cylinder.Shape(), gp_Pnt{0.0, 0.0, half_height}, 0.1);
    if (mesh.faces.size() != 3)
    {
        throw std::runtime_error("Expected three face ranges for the cylinder");
    }

    int side_faces = 0;
    for (const auto &[first_index, index_count] : mesh.faces)
    {
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
        std::println("OCCT cylinder normals verified");
        return EXIT_SUCCESS;
    }
    catch (const std::exception &error)
    {
        std::println(stderr, "occt_cylinder_check: {}", error.what());
        return EXIT_FAILURE;
    }
}
