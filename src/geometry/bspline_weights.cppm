module;

#include <gp.hxx>

export module formlight_geometry.geometry.bspline_weights;

import std;

export namespace formlight_geometry::geometry
{
// Dividing all weights by this scale preserves rational positions and derivatives.
[[nodiscard]] inline double checked_weight_scale(const std::span<const double> weights, const std::size_t pole_count)
{
    if (weights.empty() || weights.size() != pole_count)
    {
        throw std::invalid_argument("Each B-spline pole requires one weight");
    }
    if (!std::ranges::all_of(weights, [](const double w) { return std::isfinite(w) && w > 0.0; }))
    {
        throw std::invalid_argument("Every B-spline weight must be finite and greater than zero");
    }
    const auto [smallest, largest] = std::ranges::minmax(weights);
    if (smallest / largest <= gp::Resolution())
    {
        throw std::invalid_argument("The weight ratio is too small for OCCT evaluation");
    }
    return largest;
}
} // namespace formlight_geometry::geometry
