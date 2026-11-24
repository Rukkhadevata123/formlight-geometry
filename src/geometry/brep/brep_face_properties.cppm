module;

#include <BRepAdaptor_Surface.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <GeomAbs_SurfaceType.hxx>

export module occt_lab.geometry.brep.brep_face_properties;

import std;
import occt_lab.geometry.brep.brep_mesh;

export struct BRepFaceProperties
{
    std::size_t face_index;
    GeomAbs_SurfaceType surface_type;
    double area_mm2;
};

export [[nodiscard]] BRepFaceProperties inspect_brep_face(const BRepMeshData &mesh, const std::size_t face_index)
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
