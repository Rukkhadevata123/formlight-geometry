#include "bezier_patch.hpp"
#include "bspline_surface.hpp"

#include <algorithm>

namespace occt_lab::geometry
{
namespace
{
bool single_cubic_span(const BSplineSurfaceAxis &axis)
{
    return axis.degree == 3 && axis.pole_count == 4 && axis.knots.size() == 2 && axis.knots.front().multiplicity == 4 &&
           axis.knots.back().multiplicity == 4 && std::isfinite(axis.knots.front().value) &&
           std::isfinite(axis.knots.back().value) && axis.knots.front().value < axis.knots.back().value;
}
} // namespace

auto evaluate_bernstein_reference(const BSplineSurfaceDefinition &definition, const double u, const double v)
    -> std::expected<SurfaceSample, PatchError>
{
    if (!single_cubic_span(definition.u) || !single_cubic_span(definition.v))
    {
        return std::unexpected(PatchError::NotPolynomialBicubic);
    }
    // A common positive weight cancels in the rational formula, including the implicit unit weights.
    const auto &weights = definition.weights;
    if (!weights.empty() &&
        (weights.size() != definition.poles.size() || !std::isfinite(weights.front()) || weights.front() <= 0 ||
         !std::ranges::all_of(weights, [&](double w) { return w == weights.front(); })))
    {
        return std::unexpected(PatchError::NotPolynomialBicubic);
    }

    const SurfaceDomain domain{definition.u.knots.front().value, definition.u.knots.back().value,
                               definition.v.knots.front().value, definition.v.knots.back().value};
    if (!std::isfinite(u) || !std::isfinite(v))
    {
        return std::unexpected(PatchError::NonFiniteParameter);
    }
    if (!domain.contains(u, v))
    {
        return std::unexpected(PatchError::ParameterOutOfRange);
    }
    return CubicBezierPatch::create(definition.poles)
        .and_then(
            [&](const CubicBezierPatch &patch)
            {
                return patch.evaluate(unit_parameter(u, domain.u_min, domain.u_max),
                                      unit_parameter(v, domain.v_min, domain.v_max));
            });
}
} // namespace occt_lab::geometry
