module;

#include <gp_XYZ.hxx>

export module occt_lab.render.viewport.brep_face_picking;

import std;
import occt_lab.geometry.brep.brep_mesh;
import occt_lab.render.viewport.clip_transform;

namespace
{
gp_XYZ xyz(const ClipRow &r)
{
    return {r[0], r[1], r[2]};
}

// Intersect the three planes clip.component = ndc.component * clip.w in mesh coordinates.
std::optional<gp_XYZ> unproject(const ClipTransform &clip, const double x, const double y, const double depth)
{
    const auto w = xyz(clip.w);
    const auto a = xyz(clip.x) - w * x;
    const auto b = xyz(clip.y) - w * y;
    const auto c = xyz(clip.z) - w * depth;
    const auto bc = b.Crossed(c);
    const double det = a.Dot(bc);
    // Relative to the row scales: zoom and model units must not turn a valid transform into a miss.
    if (const double det_scale = a.Modulus() * b.Modulus() * c.Modulus();
        !(std::abs(det) > std::numeric_limits<double>::epsilon() * det_scale))
    {
        return std::nullopt;
    }
    return (bc * (x * clip.w[3] - clip.x[3]) + c.Crossed(a) * (y * clip.w[3] - clip.y[3]) +
            a.Crossed(b) * (depth * clip.w[3] - clip.z[3])) /
           det;
}

// Front-facing Möller–Trumbore intersection. The segment spans the near and far clip planes.
std::optional<double> intersect(const gp_XYZ &start, const gp_XYZ &segment, const gp_XYZ &a, const gp_XYZ &b,
                                const gp_XYZ &c)
{
    const auto ab = b - a;
    const auto ac = c - a;
    const auto p = segment.Crossed(ac);
    const double det = ab.Dot(p);
    if (det <= 0)
    {
        return std::nullopt;
    }
    const auto offset = start - a;
    const auto q = offset.Crossed(ab);
    const double u = offset.Dot(p);
    if (const double v = segment.Dot(q); u < 0 || v < 0 || u + v > det)
    {
        return std::nullopt;
    }
    return ac.Dot(q) / det;
}
} // namespace

// Vulkan NDC with Y increasing downwards. Returns a face index in this particular mesh.
export [[nodiscard]] std::optional<std::size_t> pick_brep_face(const BRepMeshData &mesh, const ClipTransform &transform,
                                                               const double ndc_x, const double ndc_y)
{
    if (!(ndc_x >= -1 && ndc_x <= 1 && ndc_y >= -1 && ndc_y <= 1))
    {
        return std::nullopt;
    }
    const auto start = unproject(transform, ndc_x, ndc_y, 0);
    const auto end = unproject(transform, ndc_x, ndc_y, 1);
    if (!start || !end)
    {
        return std::nullopt;
    }
    const auto segment = *end - *start;
    const auto vertex = [&](const std::size_t i) -> gp_XYZ
    {
        const auto &[x, y, z] = mesh.vertices[mesh.indices[i]].position;
        return {x, y, z};
    };
    double nearest = 1.0; // The depth buffer is cleared to 1 and uses a strict less comparison.
    std::optional<std::size_t> hit;
    for (std::size_t i = 0; i < mesh.faces.size(); ++i)
    {
        const auto &range = mesh.faces[i];
        const std::size_t first = range.first_index;
        for (auto j = first; j < first + range.index_count; j += 3)
        {
            if (const auto t = intersect(*start, segment, vertex(j), vertex(j + 1), vertex(j + 2));
                t && *t >= 0 && *t < nearest)
            {
                nearest = *t;
                hit = i;
            }
        }
    }
    return hit;
}
