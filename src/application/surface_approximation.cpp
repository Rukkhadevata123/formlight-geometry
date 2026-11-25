module;

#include <BSplCLib.hxx>
#include <math_Householder.hxx>

module formlight_geometry.application.nurbs_surface_creation;

import std;
import formlight_geometry.geometry.surfaces.bspline_surface;

namespace formlight_geometry
{
namespace
{
geometry::BSplineSurfaceAxis fit_axis(const int poles)
{
    const int degree = std::min(SurfaceCreationParameters::preferred_degree, poles - 1);
    return {.degree = degree, .pole_count = poles, .knots = geometry::make_uniform_clamped_knots(poles, degree)};
}

math_Matrix basis_matrix(const geometry::BSplineSurfaceAxis &axis, const int samples)
{
    const int order = axis.degree + 1;
    NCollection_Array1<double> flat_knots(1, axis.pole_count + order);
    int index = 1;
    for (const auto &[value, multiplicity] : axis.knots)
    {
        for (int i = 0; i < multiplicity; ++i)
        {
            flat_knots(index++) = value;
        }
    }

    math_Matrix basis(1, samples, 1, axis.pole_count, 0.0);
    math_Matrix local(1, 1, 1, order);
    for (int i = 1; i <= samples; ++i)
    {
        const double t = static_cast<double>(i - 1) / (samples - 1);
        int first = 0;
        if (BSplCLib::EvalBsplineBasis(0, order, flat_knots, t, first, local) != 0)
        {
            throw std::runtime_error("Could not evaluate the point-grid fitting basis.");
        }
        for (int j = 1; j <= order; ++j)
        {
            basis(i, first + j - 1) = local(1, j);
        }
    }
    return basis;
}

// Solve all XYZ rows against one basis. Transposing the coefficients makes them rows for the next axis.
std::vector<geometry::Vec3> fit_rows(const std::vector<geometry::Vec3> &points, const int columns, const int rows,
                                     const geometry::BSplineSurfaceAxis &axis)
{
    math_Matrix coordinates(1, columns, 1, 3 * rows);
    for (int row = 0; row < rows; ++row)
    {
        for (int i = 0; i < columns; ++i)
        {
            const auto &[x, y, z] = points[row * columns + i];
            coordinates(i + 1, 3 * row + 1) = x;
            coordinates(i + 1, 3 * row + 2) = y;
            coordinates(i + 1, 3 * row + 3) = z;
        }
    }

    const math_Householder fit(basis_matrix(axis, columns), coordinates);
    if (!fit.IsDone())
    {
        throw std::runtime_error("Could not solve the point-grid least-squares system.");
    }
    const auto &coefficients = fit.AllValues();
    std::vector<geometry::Vec3> transposed;
    transposed.reserve(axis.pole_count * rows);
    for (int i = 1; i <= axis.pole_count; ++i)
    {
        for (int row = 0; row < rows; ++row)
        {
            transposed.push_back(
                {coefficients(i, 3 * row + 1), coefficients(i, 3 * row + 2), coefficients(i, 3 * row + 3)});
        }
    }
    return transposed;
}

int refine_axis(const int poles, const int samples)
{
    // Raise the degree up to cubic, then double the number of knot spans.
    const int degree = SurfaceCreationParameters::preferred_degree;
    return std::min(samples, std::max(poles + 1, 2 * poles - degree));
}
} // namespace

auto approximate_surface_grid(const SurfacePoleRows &rows, const double tolerance_mm)
    -> std::expected<geometry::BSplineSurfaceDefinition, std::string>
{
    if (!std::isfinite(tolerance_mm) || tolerance_mm <= 0)
    {
        return std::unexpected("Enter a positive finite fitting tolerance in mm.");
    }
    auto grid = read_surface_point_grid(rows);
    if (!grid)
    {
        return std::unexpected(grid.error());
    }
    grid->fit_tolerance_mm = tolerance_mm;
    try
    {
        int nu = 2;
        int nv = 2;
        for (;;)
        {
            geometry::BSplineSurfaceDefinition candidate{};
            const bool full_grid = nu == grid->columns && nv == grid->rows;
            if (full_grid)
            {
                candidate = interpolate_grid_points(*grid);
            }
            else
            {
                candidate.u = fit_axis(nu);
                candidate.v = fit_axis(nv);
                const auto along_u = fit_rows(grid->points, grid->columns, grid->rows, candidate.u);
                candidate.poles = fit_rows(along_u, grid->rows, nu, candidate.v);
            }

            const double error = geometry::BSplineSurface{candidate}.max_grid_deviation(*grid).distance_mm;
            if (error <= tolerance_mm)
            {
                candidate.data_grid = std::move(*grid);
                return candidate;
            }
            if (full_grid)
            {
                return std::unexpected(
                    std::format("Measured error {:.4g} mm exceeds the requested {:.4g} mm.", error, tolerance_mm));
            }
            nu = refine_axis(nu, grid->columns);
            nv = refine_axis(nv, grid->rows);
        }
    }
    catch (const std::exception &error)
    {
        return std::unexpected(error.what());
    }
}
} // namespace formlight_geometry
