#include "bspline_knot.hpp"

#include <stdexcept>

namespace occt_lab::geometry
{
std::vector<BSplineKnot> make_uniform_clamped_knots(const int pole_count, const int degree)
{
    if (degree < 1 || pole_count <= degree)
    {
        throw std::invalid_argument("A clamped spline needs a positive degree and more poles than its degree");
    }
    const int spans = pole_count - degree;
    std::vector<BSplineKnot> knots;
    knots.reserve(static_cast<std::size_t>(spans) + 1);
    for (int i = 0; i <= spans; ++i)
    {
        const bool endpoint = i == 0 || i == spans;
        knots.push_back({.value = static_cast<double>(i) / spans, .multiplicity = endpoint ? degree + 1 : 1});
    }
    return knots;
}
} // namespace occt_lab::geometry
