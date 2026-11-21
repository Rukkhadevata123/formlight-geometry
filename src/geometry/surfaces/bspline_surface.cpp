#include "bspline_surface.hpp"

#include "geometry/occt_bspline_knots.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>

namespace occt_lab::geometry
{
namespace
{
bool finite(const Vec3 &p)
{
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}

OcctKnotArrays axis_knots(const BSplineSurfaceAxis &axis)
{
    if (axis.degree < 1 || axis.degree > Geom_BSplineSurface::MaxDegree() || axis.pole_count <= axis.degree)
    {
        throw std::invalid_argument("A surface axis needs a supported degree and more poles than its degree");
    }
    return make_clamped_knots(axis.knots, axis.degree, axis.pole_count);
}

Geom_BSplineSurface make_surface(const BSplineSurfaceDefinition &definition)
{
    const auto u = axis_knots(definition.u);
    const auto v = axis_knots(definition.v);
    const int columns = definition.u.pole_count;
    const int rows = definition.v.pole_count;
    const auto count = static_cast<std::int64_t>(columns) * rows;
    if (count != static_cast<std::int64_t>(definition.poles.size()))
    {
        throw std::invalid_argument("Control net dimensions do not match the number of surface poles");
    }
    // OCCT's 1-based Array2 also computes the backing array's offset using int.
    if (count + rows + 1 > std::numeric_limits<int>::max())
    {
        throw std::invalid_argument("Surface control net exceeds the OCCT array range");
    }
    NCollection_Array2<gp_Pnt> poles(1, columns, 1, rows);
    for (int row = 0; row < rows; ++row)
    {
        for (int column = 0; column < columns; ++column)
        {
            const auto &p = definition.poles[row * columns + column];
            if (!finite(p))
            {
                throw std::invalid_argument("B-spline poles must have finite coordinates");
            }
            poles.SetValue(column + 1, row + 1, gp_Pnt{p.x, p.y, p.z});
        }
    }
    return Geom_BSplineSurface{
        poles, u.values, v.values, u.multiplicities, v.multiplicities, definition.u.degree, definition.v.degree};
}

int derivative_span(const double parameter, const NCollection_Array1<double> &knots,
                    const NCollection_Array1<int> &multiplicities, const int degree)
{
    const std::span values(&knots.First(), knots.Size());
    // Search only interior knots so both domain endpoints select a valid adjacent span.
    const auto next = std::upper_bound(values.begin() + 1, values.end() - 1, parameter);
    const int first = knots.Lower() + static_cast<int>(next - values.begin()) - 1;
    if (first > knots.Lower() && parameter == knots(first) && multiplicities(first) == degree)
    {
        throw std::domain_error("First derivatives are not guaranteed at this C0 surface knot");
    }
    return first;
}
} // namespace

bool SurfaceDomain::contains(const double u, const double v) const
{
    return u >= u_min && u <= u_max && v >= v_min && v <= v_max;
}

BSplineSurface::BSplineSurface(const BSplineSurfaceDefinition &definition) : surface_(make_surface(definition))
{
}

SurfaceDomain BSplineSurface::domain() const
{
    SurfaceDomain bounds;
    surface_.Bounds(bounds.u_min, bounds.u_max, bounds.v_min, bounds.v_max);
    return bounds;
}

auto BSplineSurface::position(const double u, const double v) const -> std::expected<Vec3, std::string>
{
    if (!domain().contains(u, v))
    {
        return std::unexpected("Parameters must lie inside the surface domain");
    }
    try
    {
        const auto p = surface_.EvalD0(u, v);
        const Vec3 point{.x = p.X(), .y = p.Y(), .z = p.Z()};
        if (!finite(point))
        {
            return std::unexpected("Surface position exceeded the finite coordinate range");
        }
        return point;
    }
    catch (const std::exception &error)
    {
        return std::unexpected(error.what());
    }
}

auto BSplineSurface::evaluate_d1(const double u, const double v) const -> std::expected<SurfaceD1, std::string>
{
    if (!domain().contains(u, v))
    {
        return std::unexpected("Parameters must lie inside the surface domain");
    }
    try
    {
        const int iu = derivative_span(u, surface_.UKnots(), surface_.UMultiplicities(), surface_.UDegree());
        const int iv = derivative_span(v, surface_.VKnots(), surface_.VMultiplicities(), surface_.VDegree());
        gp_Pnt p;
        gp_Vec du, dv;
        surface_.LocalD1(u, v, iu, iu + 1, iv, iv + 1, p, du, dv);
        const SurfaceD1 sample{
            .position = {p.X(), p.Y(), p.Z()}, .du = {du.X(), du.Y(), du.Z()}, .dv = {dv.X(), dv.Y(), dv.Z()}};
        if (!finite(sample.position) || !finite(sample.du) || !finite(sample.dv))
        {
            return std::unexpected("Surface evaluation exceeded the finite coordinate range");
        }
        return sample;
    }
    catch (const std::exception &error)
    {
        return std::unexpected(error.what());
    }
}
} // namespace occt_lab::geometry
