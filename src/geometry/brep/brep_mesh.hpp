#pragma once

#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>

#include <array>
#include <cstdint>
#include <vector>

class gp_Pnt;

struct MeshVertex
{
    std::array<float, 3> position;
    std::array<float, 3> normal;
};

struct FaceIndexRange
{
    std::uint32_t first_index;
    std::uint32_t index_count;
    TopoDS_Face face;
};

struct BRepMeshData
{
    TopoDS_Shape source_shape;
    std::vector<MeshVertex> vertices;
    std::vector<std::uint32_t> indices;
    // Face ranges follow the traversal order of the source shape.
    std::vector<FaceIndexRange> faces;
};

// Meshing and normal generation can update triangulations shared by other shape handles.
[[nodiscard]] BRepMeshData triangulate_brep(const TopoDS_Shape &shape, const gp_Pnt &origin, double linear_deflection);
