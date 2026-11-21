#include "application/editable_nurbs_surface.hpp"
#include "application/nurbs_surface_creation.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace
{
void check_planar_creation()
{
    using occt_lab::PlanarSurfaceParameters;
    for (const auto [u_count, v_count] :
         {std::pair{2, 2}, std::pair{6, 3}, std::pair{3, PlanarSurfaceParameters::max_poles}})
    {
        const auto plane = occt_lab::make_planar_nurbs_surface({u_count, v_count, 120, 40}).value();
        if (plane.u.pole_count != u_count || plane.v.pole_count != v_count ||
            plane.u.degree != std::min(3, u_count - 1) || plane.v.degree != std::min(3, v_count - 1))
        {
            throw std::runtime_error("Planar surface axes do not match the requested grid");
        }
        for (int v = 0; v < v_count; ++v)
        {
            for (int u = 0; u < u_count; ++u)
            {
                const auto index = static_cast<std::size_t>(v * u_count + u);
                const auto &[x, y, z] = plane.poles.at(index);
                if (std::abs(x - (-60 + 120.0 * u / (u_count - 1))) > 1e-10 ||
                    std::abs(y - (-20 + 40.0 * v / (v_count - 1))) > 1e-10 || z != 0 || plane.weights.at(index) != 1)
                {
                    throw std::runtime_error("Planar control net lost its spacing, row order or unit weights");
                }
            }
        }
        const occt_lab::geometry::BSplineSurface surface{plane};
        const auto near = [](const auto &a, const occt_lab::geometry::Vec3 &b)
        { return std::hypot(a.x - b.x, a.y - b.y, a.z - b.z) <= 1e-10; };
        if (!near(surface.position(0.5, 0.5).value(), {}) ||
            !near(surface.position(0, 0).value(), plane.poles.front()) ||
            !near(surface.position(1, 1).value(), plane.poles.back()))
        {
            throw std::runtime_error("Created plane does not interpolate its corners or span the origin");
        }
    }

    for (const auto invalid : {PlanarSurfaceParameters{1, 4, 120, 40}, PlanarSurfaceParameters{4, 65, 120, 40},
                               PlanarSurfaceParameters{4, 4, 0, 40}, PlanarSurfaceParameters{4, 4, 120, -1},
                               PlanarSurfaceParameters{4, 4, std::numeric_limits<double>::quiet_NaN(), 40},
                               PlanarSurfaceParameters{4, 4, 120, std::numeric_limits<double>::infinity()}})
    {
        if (occt_lab::make_planar_nurbs_surface(invalid))
        {
            throw std::runtime_error("New surface accepted invalid grid dimensions");
        }
    }
}
} // namespace

void check_nurbs_presets()
{
    check_planar_creation();
    const auto saddle = occt_lab::make_saddle_surface();
    const occt_lab::geometry::BSplineSurface surface{saddle};
    for (const double u : {0.0, 0.25, 0.5, 0.75, 1.0})
    {
        for (const double v : {0.0, 0.25, 0.5, 0.75, 1.0})
        {
            const double s = 2 * u - 1;
            const double t = 2 * v - 1;
            // The center pole is at the origin. Its extra unit of weight changes only the denominator.
            const double w = 1 + 4 * u * (1 - u) * v * (1 - v);
            const auto p = surface.position(u, v).value();
            if (!(std::hypot(p.x - 45 * s / w, p.y - 36 * t / w, p.z - 12 * (s * s - t * t) / w) <= 1e-10))
            {
                throw std::runtime_error("Saddle preset differs from its rational quadratic surface");
            }
        }
    }

    EditableNurbsSurface model{saddle};
    const auto saddle_frame = model.current().framing;
    auto poles = saddle.poles;
    poles.back().x += 10;
    model.apply(poles, saddle.weights).value();
    if (model.current().framing != saddle_frame)
    {
        throw std::runtime_error("Editing an outer saddle pole changed the framing");
    }
    const auto edited = model.current();
    const auto plane = occt_lab::make_planar_nurbs_surface({6, 3, 120, 40}).value();
    model.replace(plane).value();
    const auto plane_frame = model.current().framing;
    if (model.current().definition != plane || plane_frame == saddle_frame)
    {
        throw std::runtime_error("Creating a plane did not replace the geometry and framing");
    }
    model.replace(plane).value();
    model.undo();
    if (model.current().definition != edited.definition || model.current().framing != saddle_frame ||
        model.current().isolines.constant_v != edited.isolines.constant_v)
    {
        throw std::runtime_error("Undo should restore the edited saddle, its isolines and its original framing");
    }
    auto invalid = plane;
    invalid.poles.pop_back();
    if (model.replace(std::move(invalid)) || !model.can_redo() || model.current().definition != edited.definition)
    {
        throw std::runtime_error("An invalid replacement disturbed the edited saddle or its redo step");
    }
    model.redo();
    if (model.current().definition != plane || model.current().framing != plane_frame)
    {
        throw std::runtime_error("Redo should restore the created plane and its framing");
    }
}
