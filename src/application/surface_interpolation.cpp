module;

#include <BSplCLib.hxx>
#include <gp_Pnt.hxx>

module occt_lab.application.nurbs_surface_creation;

import std;
import occt_lab.geometry.surfaces.bspline_surface;

namespace occt_lab
{
namespace
{
// Each solve replaces a line of samples with its B-spline coefficients, in place.
geometry::BSplineSurfaceAxis interpolate_axis(std::vector<geometry::Vec3> &points, const int count,
                                              const int line_count, const int line_step, const int point_step)
{
    const int degree = std::min(count - 1, SurfaceCreationParameters::preferred_degree);
    NCollection_Array1<double> parameters(1, count);
    for (int i = 1; i <= count; ++i)
    {
        parameters(i) = static_cast<double>(i - 1) / (count - 1);
    }

    // Average consecutive interpolation parameters to obtain the interior knots.
    geometry::BSplineSurfaceAxis axis{.degree = degree, .pole_count = count, .knots = {{0, degree + 1}}};
    for (int first = 2; first <= count - degree; ++first)
    {
        double sum = 0;
        for (int i = first; i < first + degree; ++i)
        {
            sum += parameters(i);
        }
        axis.knots.push_back({sum / degree, 1});
    }
    axis.knots.push_back({1, degree + 1});

    NCollection_Array1<double> flat_knots(1, count + degree + 1);
    int index = 1;
    for (const auto &[value, multiplicity] : axis.knots)
    {
        for (int i = 0; i < multiplicity; ++i)
        {
            flat_knots(index++) = value;
        }
    }
    NCollection_Array1<int> contact_orders(1, count);
    contact_orders.Init(0); // Positional constraints at every site.
    NCollection_Array1<gp_Pnt> poles(1, count);
    for (int line = 0; line < line_count; ++line)
    {
        auto point = [&](int i) -> geometry::Vec3 & { return points[line * line_step + i * point_step]; };
        for (int i = 0; i < count; ++i)
        {
            const auto &[x, y, z] = point(i);
            poles(i + 1) = gp_Pnt{x, y, z};
        }
        int bad_pivot = 0;
        BSplCLib::Interpolate(degree, flat_knots, parameters, contact_orders, poles, bad_pivot);
        if (bad_pivot != 0)
        {
            throw std::runtime_error("Could not solve the point-grid interpolation system.");
        }
        for (int i = 0; i < count; ++i)
        {
            const auto &p = poles(i + 1);
            if (!std::isfinite(p.X()) || !std::isfinite(p.Y()) || !std::isfinite(p.Z()))
            {
                throw std::runtime_error("Point-grid interpolation exceeded the finite coordinate range.");
            }
            point(i) = {p.X(), p.Y(), p.Z()};
        }
    }
    return axis;
}
} // namespace

auto read_surface_point_grid(const SurfacePoleRows &rows) -> std::expected<geometry::SurfacePointGrid, std::string>
{
    const auto valid_count = [](std::size_t n)
    { return n >= SurfaceCreationParameters::min_poles && n <= SurfaceCreationParameters::max_poles; };
    if (!valid_count(rows.size()) || !valid_count(rows.front().size()))
    {
        return std::unexpected("Choose 2 to 64 data points in each direction.");
    }
    const int nu = static_cast<int>(rows.front().size());
    const int nv = static_cast<int>(rows.size());
    geometry::SurfacePointGrid grid{.columns = nu, .rows = nv};
    grid.points.reserve(nu * nv);
    for (const auto &row : rows)
    {
        if (row.size() != rows.front().size())
        {
            return std::unexpected("Each V row must have the same number of U data points.");
        }
        for (const auto &cell : row)
        {
            const auto &[x, y, z] = cell.position;
            if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
            {
                const auto i = grid.points.size();
                return std::unexpected(
                    std::format("U {}, V {}: enter finite XYZ coordinates.", i % nu + 1, i / nu + 1));
            }
            grid.points.push_back(cell.position);
        }
    }
    return grid;
}

geometry::BSplineSurfaceDefinition interpolate_grid_points(const geometry::SurfacePointGrid &grid)
{
    geometry::BSplineSurfaceDefinition result{.poles = grid.points};
    result.u = interpolate_axis(result.poles, grid.columns, grid.rows, grid.columns, 1);
    result.v = interpolate_axis(result.poles, grid.rows, grid.columns, 1, grid.columns);
    return result;
}

auto interpolate_surface_grid(const SurfacePoleRows &rows)
    -> std::expected<geometry::BSplineSurfaceDefinition, std::string>
{
    auto grid = read_surface_point_grid(rows);
    if (!grid)
    {
        return std::unexpected(grid.error());
    }
    try
    {
        auto result = interpolate_grid_points(*grid);
        result.data_grid = std::move(*grid);
        return result;
    }
    catch (const std::exception &error)
    {
        return std::unexpected(error.what());
    }
}
} // namespace occt_lab
