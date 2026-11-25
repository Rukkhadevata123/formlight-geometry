module;

#include <Geom_BSplineSurface.hxx>
#include <Precision.hxx>

export module formlight_geometry.application.nurbs_surface_creation;

import std;
import formlight_geometry.geometry.surfaces.bezier_patch;
import formlight_geometry.geometry.surfaces.bspline_surface;

namespace formlight_geometry
{
export struct SurfaceCreationParameters
{
    static constexpr int min_poles = 2;
    static constexpr int max_poles = 64;
    static constexpr int preferred_degree = 3;
    [[nodiscard]] static int degree_limit(int pole_count);
    int u_poles = 4;
    int v_poles = 4;
    double width_mm = 90;
    double height_mm = 72;
    // An unset degree follows the default for that direction's pole count.
    std::optional<int> u_degree{};
    std::optional<int> v_degree{};
    // Each axis defaults to uniform clamped knots until a custom vector is supplied.
    std::optional<std::vector<geometry::BSplineKnot>> u_knots{};
    std::optional<std::vector<geometry::BSplineKnot>> v_knots{};
};

export struct WeightedSurfacePole
{
    geometry::Vec3 position{};
    double weight = 1;
    bool operator==(const WeightedSurfacePole &) const = default;
};

// Outer rows follow V; columns within each row follow U.
export using SurfacePoleRows = std::vector<std::vector<WeightedSurfacePole>>;

export [[nodiscard]] auto interpolate_surface_grid(const SurfacePoleRows &rows)
    -> std::expected<geometry::BSplineSurfaceDefinition, std::string>;

export [[nodiscard]] auto approximate_surface_grid(const SurfacePoleRows &rows, double tolerance_mm)
    -> std::expected<geometry::BSplineSurfaceDefinition, std::string>;

auto read_surface_point_grid(const SurfacePoleRows &rows) -> std::expected<geometry::SurfacePointGrid, std::string>;
geometry::BSplineSurfaceDefinition interpolate_grid_points(const geometry::SurfacePointGrid &grid);

using geometry::BSplineSurfaceDefinition;

int SurfaceCreationParameters::degree_limit(const int pole_count)
{
    return pole_count > 1 ? std::min(pole_count - 1, Geom_BSplineSurface::MaxDegree()) : 0;
}

namespace
{
auto make_creation_axis(const char *direction, const int poles, const std::optional<int> requested_degree,
                        const std::optional<std::vector<geometry::BSplineKnot>> &custom_knots)
    -> std::expected<geometry::BSplineSurfaceAxis, std::string>
{
    const int limit = SurfaceCreationParameters::degree_limit(poles);
    const int degree = requested_degree.value_or(std::min(SurfaceCreationParameters::preferred_degree, limit));
    if (degree < 1 || degree > limit)
    {
        return std::unexpected(std::format("With {} {} poles, the {} degree must be between 1 and {}.", poles,
                                           direction, direction, limit));
    }
    if (custom_knots)
    {
        if (const auto valid = geometry::validate_clamped_knots(*custom_knots, degree, poles); !valid)
        {
            return std::unexpected(std::format("{} knots: {}", direction, valid.error()));
        }
    }
    return geometry::BSplineSurfaceAxis{.degree = degree,
                                        .pole_count = poles,
                                        .knots = custom_knots ? *custom_knots
                                                              : geometry::make_uniform_clamped_knots(poles, degree)};
}

bool valid_pole_count(const int count)
{
    return count >= SurfaceCreationParameters::min_poles && count <= SurfaceCreationParameters::max_poles;
}

auto make_surface_axes(const SurfaceCreationParameters &parameters)
    -> std::expected<BSplineSurfaceDefinition, std::string>
{
    if (!valid_pole_count(parameters.u_poles) || !valid_pole_count(parameters.v_poles))
    {
        return std::unexpected(std::format("Choose {} to {} poles per direction.", SurfaceCreationParameters::min_poles,
                                           SurfaceCreationParameters::max_poles));
    }
    auto u = make_creation_axis("U", parameters.u_poles, parameters.u_degree, parameters.u_knots);
    if (!u)
    {
        return std::unexpected(u.error());
    }
    auto v = make_creation_axis("V", parameters.v_poles, parameters.v_degree, parameters.v_knots);
    if (!v)
    {
        return std::unexpected(v.error());
    }
    return BSplineSurfaceDefinition{.u = std::move(*u), .v = std::move(*v)};
}
} // namespace

export void resize_surface_net(SurfacePoleRows &net, const int columns, const int rows)
{
    if (!valid_pole_count(columns) || !valid_pole_count(rows))
    {
        throw std::invalid_argument("Control net dimensions are outside the supported creation range.");
    }
    net.resize(rows);
    for (int v = 0; v < rows; ++v)
    {
        auto &row = net[v];
        const int previous = static_cast<int>(row.size());
        row.resize(columns);
        for (int u = previous; u < columns; ++u)
        {
            // Fixed spacing keeps existing U/V addresses and positions stable when the grid grows.
            row[u].position = {30.0 * u - 45, 36.0 * v - 36, 0};
        }
    }
}

export [[nodiscard]] auto make_control_net_surface(const SurfaceCreationParameters &parameters,
                                                   const SurfacePoleRows &net)
    -> std::expected<BSplineSurfaceDefinition, std::string>
{
    auto definition = make_surface_axes(parameters);
    if (!definition)
    {
        return definition;
    }
    if (net.size() != static_cast<std::size_t>(parameters.v_poles))
    {
        return std::unexpected("The number of control net rows must match V poles.");
    }
    const auto count = definition->control_net_size();
    definition->poles.reserve(count);
    definition->weights.reserve(count);
    for (const auto &row : net)
    {
        if (row.size() != static_cast<std::size_t>(parameters.u_poles))
        {
            return std::unexpected("Every control net row must contain U poles.");
        }
        for (const auto &[position, weight] : row)
        {
            if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z) ||
                !std::isfinite(weight) || weight <= 0)
            {
                const auto index = definition->poles.size();
                return std::unexpected(std::format("U {}, V {}: enter finite XYZ and a positive finite weight.",
                                                   index % row.size() + 1, index / row.size() + 1));
            }
            definition->poles.push_back(position);
            definition->weights.push_back(weight);
        }
    }
    return definition;
}

export [[nodiscard]] auto make_planar_nurbs_surface(const SurfaceCreationParameters &parameters)
    -> std::expected<BSplineSurfaceDefinition, std::string>
{
    auto definition = make_surface_axes(parameters);
    if (!definition)
    {
        return definition;
    }
    const int columns = parameters.u_poles, rows = parameters.v_poles;
    const double width = parameters.width_mm, height = parameters.height_mm;
    const auto valid_length = [](double mm)
    { return std::isfinite(mm) && mm > Precision::Confusion() && !Precision::IsInfinite(mm); };
    if (!valid_length(width) || !valid_length(height))
    {
        return std::unexpected(std::format("Width and height must be greater than {:g} mm and below {:g} mm.",
                                           Precision::Confusion(), 0.5 * Precision::Infinite()));
    }
    const auto count = static_cast<std::size_t>(rows) * columns;
    definition->poles.reserve(count);
    definition->weights.assign(count, 1.0);
    for (int row = 0; row < rows; ++row)
    {
        const double y = std::lerp(-height / 2, height / 2, static_cast<double>(row) / (rows - 1));
        for (int column = 0; column < columns; ++column)
        {
            const double x = std::lerp(-width / 2, width / 2, static_cast<double>(column) / (columns - 1));
            definition->poles.push_back({.x = x, .y = y, .z = 0});
        }
    }
    return definition;
}

export [[nodiscard]] BSplineSurfaceDefinition make_bezier_nurbs_surface(
    const geometry::CubicBezierPatch::ControlNet &net)
{
    const geometry::BSplineSurfaceAxis cubic{.degree = 3, .pole_count = 4, .knots = {{0, 4}, {1, 4}}};
    return {.u = cubic, .v = cubic, .poles = {net.begin(), net.end()}, .weights = std::vector<double>(net.size(), 1.0)};
}

export [[nodiscard]] BSplineSurfaceDefinition make_saddle_surface()
{
    const geometry::BSplineSurfaceAxis axis{.degree = 2, .pole_count = 3, .knots = {{0, 3}, {1, 3}}};
    BSplineSurfaceDefinition definition{.u = axis, .v = axis};
    // In the quadratic Bernstein basis, (2t-1)^2 has coefficients 1, -1, 1.
    constexpr std::array square{1.0, -1.0, 1.0};
    for (int v = 0; v < axis.pole_count; ++v)
    {
        for (int u = 0; u < axis.pole_count; ++u)
        {
            definition.poles.push_back({.x = 45.0 * (u - 1), .y = 36.0 * (v - 1), .z = 12.0 * (square[u] - square[v])});
            definition.weights.push_back(u == 1 && v == 1 ? 2.0 : 1.0);
        }
    }
    return definition;
}
} // namespace formlight_geometry
