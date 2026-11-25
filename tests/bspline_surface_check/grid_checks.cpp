import std;
import formlight_geometry.geometry.surfaces.bspline_surface;
import formlight_geometry.render.viewport.clip_transform;
import formlight_geometry.test.bspline_surface_check.analytic_surface;

namespace
{
using namespace formlight_geometry::geometry;

void check_rectangular_grid(BSplineSurfaceDefinition definition)
{
    definition.weights.clear();
    const BSplineSurface surface{definition};
    const auto vertices = surface.sample_grid({3, 8}).value();
    if (vertices.size() != 36)
    {
        throw std::runtime_error("A 3 by 8 subdivision grid should contain 4 by 9 vertices");
    }
    for (std::size_t index = 0; index < vertices.size(); ++index)
    {
        const double u = 2.0 + static_cast<double>(index % 4);
        const double v = -2.0 + 0.5 * static_cast<double>(index / 4);
        const double z = u * u + u * v + std::max(0.0, v - 0.5);
        const auto &[point, normal] = vertices[index];
        if (!(std::hypot(point.x - u, point.y - v, point.z - z) <= 1e-10))
        {
            throw std::runtime_error(std::format("Grid position differs from the analytic surface at ({}, {})", u, v));
        }
        if (v == 0.5)
        {
            if (normal)
            {
                throw std::runtime_error("The crease row should retain its positions with unavailable normals");
            }
            continue;
        }
        const double nx = -(2 * u + v);
        const double ny = -(u + (v > 0.5 ? 1.0 : 0.0));
        const double length = std::hypot(nx, ny, 1.0);
        if (!normal ||
            !(std::hypot(normal->x - nx / length, normal->y - ny / length, normal->z - 1.0 / length) <= 1e-10))
        {
            throw std::runtime_error(std::format("Incorrect oriented grid normal at ({}, {})", u, v));
        }
    }
}
} // namespace

void check_nurbs_reference_grid()
{
    const auto definition = formlight_geometry::test::make_analytic_surface();
    check_rectangular_grid(definition);

    const BSplineSurface surface{definition};
    const auto vertices = surface.sample_grid({3, 16}).value();
    const auto &point = vertices.at(5 * 4 + 1).position; // (u, v) = (3, -0.75), pole weight 7.
    if (!(std::hypot(point.x - 3.5, point.y + 0.125, point.z - 11.875) <= 1e-10))
    {
        throw std::runtime_error("Grid sampling lost the rational control point weight");
    }

    for (const auto grid : {PatchGrid{1, 1}, PatchGrid{PatchGrid::max_segments, PatchGrid::max_segments}})
    {
        const auto samples = surface.sample_grid(grid).value();
        if (samples.size() != grid.vertex_count())
        {
            throw std::runtime_error("A supported grid returned the wrong number of vertices");
        }
    }
    for (const auto grid : {PatchGrid{0, 1}, PatchGrid{1, 0}, PatchGrid{PatchGrid::max_segments + 1, 1},
                            PatchGrid{1, std::numeric_limits<std::uint32_t>::max()}})
    {
        if (surface.sample_grid(grid))
        {
            throw std::runtime_error("Accepted an unsupported reference grid size");
        }
    }
}

void check_nurbs_mesh_drawing()
{
    const auto indices = PatchGrid{2, 1}.triangle_indices();
    if (indices != std::vector<std::uint32_t>{0, 1, 4, 0, 4, 3, 1, 2, 5, 1, 5, 4})
    {
        throw std::runtime_error("Surface triangles lost U-first ordering or positive UV winding");
    }
    for (const auto grid : {PatchGrid{1, 1}, PatchGrid{}, PatchGrid{32, 32}})
    {
        const auto triangles = grid.triangle_indices();
        if (triangles.size() != grid.index_count() || std::ranges::max(triangles) != grid.vertex_count() - 1)
        {
            throw std::runtime_error("Surface index range does not match the vertex grid");
        }
    }

    // Keep the pole origin and the fitted center distinct, including after a large world translation.
    constexpr Vec3 origin{1e12 + 17, -1e12 - 3, 2e12 + 9};
    constexpr Vec3 center{1e12, -1e12, 2e12};
    constexpr Vec3 local{1, -5, 13};
    constexpr double scale = 2;
    constexpr ClipTransform camera{{1, 2, 0, -3}, {-1, 0, 2, 7}, {0, 1, -1, 3}, {0.5F, 0, -1, 40}};
    const auto mesh = camera.from_local_frame(origin, center, scale);
    const auto apply = [](const ClipRow &r, Vec3 p) { return r[0] * p.x + r[1] * p.y + r[2] * p.z + r[3]; };
    const Vec3 wire{(origin.x + local.x - center.x) / scale, (origin.y + local.y - center.y) / scale,
                    (origin.z + local.z - center.z) / scale};
    if (apply(mesh.x, local) != apply(camera.x, wire) || apply(mesh.y, local) != apply(camera.y, wire) ||
        apply(mesh.z, local) != apply(camera.z, wire) || apply(mesh.w, local) != apply(camera.w, wire))
    {
        throw std::runtime_error("The locally framed GPU vertex does not align with the CPU wireframe in clip space");
    }
}
