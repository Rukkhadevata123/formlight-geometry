#include "brep_face_properties.hpp"

#include "brep_mesh.hpp"

#include <BRepAdaptor_Surface.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>

BRepFaceProperties inspect_brep_face(const BRepMeshData &mesh, const std::size_t face_index)
{
    const auto &face = mesh.faces.at(face_index).face;
    GProp_GProps properties;
    BRepGProp::SurfaceProperties(face, properties);
    return {
        .face_index = face_index,
        .surface_type = BRepAdaptor_Surface{face}.GetType(),
        .area_mm2 = properties.Mass(),
    };
}
