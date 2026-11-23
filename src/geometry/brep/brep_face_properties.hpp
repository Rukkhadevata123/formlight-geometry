#pragma once

#include <GeomAbs_SurfaceType.hxx>

#include <cstddef>

struct BRepMeshData;

struct BRepFaceProperties
{
    std::size_t face_index;
    GeomAbs_SurfaceType surface_type;
    double area_mm2;
};

[[nodiscard]] BRepFaceProperties inspect_brep_face(const BRepMeshData &mesh, std::size_t face_index);
