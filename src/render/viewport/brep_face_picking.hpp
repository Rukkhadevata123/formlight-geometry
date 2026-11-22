#pragma once

#include <cstddef>
#include <optional>

struct BRepMeshData;
struct ClipTransform;

// Vulkan NDC with Y increasing downwards. Returns a face index in this particular mesh.
[[nodiscard]] std::optional<std::size_t> pick_brep_face(const BRepMeshData &mesh, const ClipTransform &transform,
                                                        double ndc_x, double ndc_y);
