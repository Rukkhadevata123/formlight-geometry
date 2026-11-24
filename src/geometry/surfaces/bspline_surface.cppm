module;

#include <Geom_BSplineSurface.hxx>
#include <Precision.hxx>

export module occt_lab.geometry.surfaces.bspline_surface;

import std;
export import occt_lab.geometry.bspline_knot;
export import occt_lab.geometry.surfaces.patch_grid;
export import occt_lab.geometry.surfaces.surface_isolines;
import occt_lab.geometry.bspline_weights;
import occt_lab.geometry.occt_bspline_knots;

namespace occt_lab::geometry
{
export struct BSplineSurfaceAxis
{
    int degree;
    int pole_count;
    std::vector<BSplineKnot> knots;
    bool operator==(const BSplineSurfaceAxis &) const = default;
};

export struct BSplineSurfaceDefinition
{
    BSplineSurfaceAxis u;
    BSplineSurfaceAxis v;
    std::vector<Vec3> poles;     // v rows, with u varying fastest.
    std::vector<double> weights; // Same order as poles. An empty array assigns unit weights.
    [[nodiscard]] std::size_t control_net_size() const;
    bool operator==(const BSplineSurfaceDefinition &) const = default;
};

export struct SurfaceD1
{
    Vec3 position;
    Vec3 du;
    Vec3 dv;
};

export struct SurfaceProbe
{
    Vec3 position;
    std::expected<Vec3, std::string> normal;
};

// A non-periodic, clamped B-spline or NURBS surface.
export class BSplineSurface
{
  public:
    explicit BSplineSurface(const BSplineSurfaceDefinition &definition);
    [[nodiscard]] SurfaceDomain domain() const;
    [[nodiscard]] auto position(double u, double v) const -> std::expected<Vec3, std::string>;
    [[nodiscard]] auto isolines(PatchSampling sampling = {}) const -> std::expected<PatchIsolines, std::string>;
    // Endpoints use the interior derivative. An interior knot with only C0 continuity is rejected.
    [[nodiscard]] auto evaluate_d1(double u, double v) const -> std::expected<SurfaceD1, std::string>;
    [[nodiscard]] auto probe(double u, double v) const -> std::expected<SurfaceProbe, std::string>;
    // V rows with U varying fastest, including all four boundaries of the knot domain.
    [[nodiscard]] auto sample_grid(PatchGrid grid) const -> std::expected<std::vector<SurfaceProbe>, std::string>;

  private:
    Geom_BSplineSurface surface_;
};

std::size_t BSplineSurfaceDefinition::control_net_size() const
{
    const auto count = static_cast<std::int64_t>(u.pole_count) * v.pole_count;
    // OCCT's 1-based Array2 also computes the backing array's offset using int.
    if (u.pole_count < 2 || v.pole_count < 2 || count + v.pole_count + 1 > std::numeric_limits<int>::max())
    {
        throw std::invalid_argument("Surface control net dimensions are outside the OCCT array range");
    }
    return static_cast<std::size_t>(count);
}

namespace
{
bool finite(const Vec3 &p)
{
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}

gp_Vec tangent_direction(const Vec3 &tangent)
{
    // Rescale before taking the length so the knot interval cannot overflow or underflow the norm.
    const double scale = std::max({std::abs(tangent.x), std::abs(tangent.y), std::abs(tangent.z)});
    if (scale == 0.0)
    {
        return {};
    }
    return gp_Vec{tangent.x / scale, tangent.y / scale, tangent.z / scale}.Normalized();
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
    if (definition.control_net_size() != definition.poles.size())
    {
        throw std::invalid_argument("Control net dimensions do not match the number of surface poles");
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
    if (definition.weights.empty())
    {
        return Geom_BSplineSurface{
            poles, u.values, v.values, u.multiplicities, v.multiplicities, definition.u.degree, definition.v.degree};
    }

    const double scale = checked_weight_scale(definition.weights, definition.poles.size());
    NCollection_Array2<double> weights(1, columns, 1, rows);
    for (int row = 0; row < rows; ++row)
    {
        for (int column = 0; column < columns; ++column)
        {
            weights.SetValue(column + 1, row + 1, definition.weights[row * columns + column] / scale);
        }
    }
    return Geom_BSplineSurface{poles,
                               weights,
                               u.values,
                               v.values,
                               u.multiplicities,
                               v.multiplicities,
                               definition.u.degree,
                               definition.v.degree};
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

BSplineSurface::BSplineSurface(const BSplineSurfaceDefinition &definition) : surface_(make_surface(definition))
{
}

SurfaceDomain BSplineSurface::domain() const
{
    SurfaceDomain bounds{};
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

auto BSplineSurface::isolines(const PatchSampling sampling) const -> std::expected<PatchIsolines, std::string>
{
    return sample_surface_isolines(domain(), sampling, std::string{"Invalid NURBS isoline sampling counts"},
                                   [this](double u, double v) { return position(u, v); });
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

auto BSplineSurface::probe(const double u, const double v) const -> std::expected<SurfaceProbe, std::string>
{
    const auto sample = evaluate_d1(u, v);
    if (!sample)
    {
        // A crease can still supply a point even though it has no unique normal.
        const auto point = position(u, v);
        if (!point)
        {
            return std::unexpected(point.error());
        }
        return SurfaceProbe{*point, std::unexpected(sample.error())};
    }

    const auto normal = tangent_direction(sample->du).Crossed(tangent_direction(sample->dv));
    const double length = normal.Magnitude();
    if (length <= Precision::Angular())
    {
        return SurfaceProbe{sample->position, std::unexpected("Surface tangents vanish or are parallel.")};
    }
    return SurfaceProbe{sample->position, Vec3{normal.X() / length, normal.Y() / length, normal.Z() / length}};
}

auto BSplineSurface::sample_grid(const PatchGrid grid) const -> std::expected<std::vector<SurfaceProbe>, std::string>
{
    if (!grid.valid())
    {
        return std::unexpected(std::format("Choose between 1 and {} subdivisions per axis.", PatchGrid::max_segments));
    }

    const auto bounds = domain();
    std::vector<SurfaceProbe> vertices;
    vertices.reserve(grid.vertex_count());
    for (std::uint32_t row = 0; row <= grid.v_segments; ++row)
    {
        const double v = std::lerp(bounds.v_min, bounds.v_max, static_cast<double>(row) / grid.v_segments);
        for (std::uint32_t column = 0; column <= grid.u_segments; ++column)
        {
            const double u = std::lerp(bounds.u_min, bounds.u_max, static_cast<double>(column) / grid.u_segments);
            auto sample = probe(u, v);
            if (!sample)
            {
                return std::unexpected(std::format("Grid vertex ({}, {}) at (u={}, v={}): {}", column, row, u, v,
                                                   sample.error()));
            }
            vertices.push_back(std::move(*sample));
        }
    }
    return vertices;
}
} // namespace occt_lab::geometry
