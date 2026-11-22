#include "brep_mesh.hpp"

#include <BRepLib_ToolTriangulatedShape.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <IMeshData_Status.hxx>
#include <Poly_Triangle.hxx>
#include <Poly_Triangulation.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <stdexcept>

namespace
{
constexpr std::size_t max_mesh_size = std::numeric_limits<std::uint32_t>::max();

std::array<float, 3> to_gpu_coordinates(const gp_XYZ &coordinates)
{
    constexpr double limit = std::numeric_limits<float>::max();
    const std::array values{coordinates.X(), coordinates.Y(), coordinates.Z()};
    if (std::ranges::any_of(values,
                            [](const double value) { return !std::isfinite(value) || std::abs(value) > limit; }))
    {
        throw std::runtime_error("Mesh coordinates exceed the GPU float range");
    }
    return {static_cast<float>(values[0]), static_cast<float>(values[1]), static_cast<float>(values[2])};
}

void append_face(BRepMeshData &mesh, const TopoDS_Face &face, const gp_Pnt &origin)
{
    TopLoc_Location location;
    const auto &triangulation = BRep_Tool::Triangulation(face, location);
    if (triangulation.IsNull() || triangulation->NbNodes() <= 0 || triangulation->NbTriangles() <= 0)
    {
        throw std::runtime_error("OCCT returned an empty face triangulation");
    }
    if (face.Orientation() != TopAbs_FORWARD && face.Orientation() != TopAbs_REVERSED)
    {
        throw std::runtime_error("Mesh extraction requires a forward or reversed face");
    }

    const auto node_count = static_cast<std::size_t>(triangulation->NbNodes());
    const auto triangle_count = static_cast<std::size_t>(triangulation->NbTriangles());
    if (node_count > max_mesh_size - mesh.vertices.size() || triangle_count > (max_mesh_size - mesh.indices.size()) / 3)
    {
        throw std::runtime_error("B-Rep mesh exceeds the 32-bit draw limits");
    }

    const auto base_vertex = static_cast<std::uint32_t>(mesh.vertices.size());
    const auto first_index = static_cast<std::uint32_t>(mesh.indices.size());
    const auto &transform = location.Transformation();
    const bool face_reversed = face.Orientation() == TopAbs_REVERSED;
    const bool reverse_winding = face_reversed != transform.IsNegative();
    std::vector<gp_XYZ> positions(node_count);
    std::vector<gp_XYZ> normals(node_count);
    for (std::size_t node = 0; node < node_count; ++node)
    {
        const auto position = triangulation->Node(static_cast<int>(node + 1)).Transformed(transform);
        positions[node] = position.XYZ() - origin.XYZ();
    }

    for (std::size_t triangle = 0; triangle < triangle_count; ++triangle)
    {
        auto a = 0;
        auto b = 0;
        auto c = 0;
        triangulation->Triangle(static_cast<int>(triangle + 1)).Get(a, b, c);
        if (const std::array nodes{a, b, c};
            std::ranges::any_of(nodes, [node_count](const int node)
                                { return node <= 0 || static_cast<std::size_t>(node) > node_count; }))
        {
            throw std::runtime_error("OCCT triangle contains an invalid node number");
        }
        if (reverse_winding)
        {
            std::swap(b, c);
        }

        const auto first = static_cast<std::uint32_t>(a - 1);
        const auto second = static_cast<std::uint32_t>(b - 1);
        const auto third = static_cast<std::uint32_t>(c - 1);
        const auto normal = (positions[second] - positions[first]).Crossed(positions[third] - positions[first]);
        if (!std::isfinite(normal.SquareModulus()) || normal.SquareModulus() == 0.0)
        {
            throw std::runtime_error("OCCT face contains a degenerate triangle");
        }

        for (const auto index : {first, second, third})
        {
            normals[index] += normal;
            mesh.indices.push_back(base_vertex + index);
        }
    }

    // Evaluating the surface gives matching normals on both sides of a periodic seam.
    if (triangulation->HasUVNodes())
    {
        BRepLib_ToolTriangulatedShape::ComputeNormals(face, triangulation);
    }

    for (std::size_t node = 0; node < node_count; ++node)
    {
        const double length = normals[node].Modulus();
        if (!std::isfinite(length) || length == 0.0)
        {
            throw std::runtime_error("OCCT face has a node without a usable normal");
        }
        auto normal = normals[node] / length;
        if (triangulation->HasNormals())
        {
            auto direction = triangulation->Normal(static_cast<int>(node + 1)).Transformed(transform);
            if (face_reversed)
            {
                direction.Reverse();
            }
            normal = direction.XYZ();
        }
        mesh.vertices.push_back({
            .position = to_gpu_coordinates(positions[node]),
            .normal = to_gpu_coordinates(normal),
        });
    }
    mesh.faces.push_back({
        .first_index = first_index,
        .index_count = static_cast<std::uint32_t>(mesh.indices.size() - first_index),
        .face = face,
    });
}
} // namespace

BRepMeshData triangulate_brep(const TopoDS_Shape &shape, const gp_Pnt &origin, const double linear_deflection)
{
    if (shape.IsNull() || !std::isfinite(linear_deflection) || linear_deflection <= 0.0)
    {
        throw std::runtime_error("B-Rep meshing requires a shape and positive finite deflection");
    }

    const BRepMesh_IncrementalMesh mesher{shape, linear_deflection};
    const auto status = mesher.GetStatusFlags();
    if (constexpr auto informational_status = IMeshData_ReMesh | IMeshData_Reused;
        !mesher.IsDone() || (status & ~informational_status) != IMeshData_NoError)
    {
        throw std::runtime_error(std::format("OCCT B-Rep meshing failed (status 0x{:x})", status));
    }

    BRepMeshData mesh;
    for (TopExp_Explorer faces{shape, TopAbs_FACE}; faces.More(); faces.Next())
    {
        try
        {
            append_face(mesh, TopoDS::Face(faces.Current()), origin);
        }
        catch (const std::exception &error)
        {
            throw std::runtime_error(std::format("Face {} mesh extraction: {}", mesh.faces.size() + 1, error.what()));
        }
    }
    if (mesh.faces.empty())
    {
        throw std::runtime_error("B-Rep mesh has no faces");
    }
    return mesh;
}
