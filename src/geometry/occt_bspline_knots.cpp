#include "occt_bspline_knots.hpp"

#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace occt_lab::geometry
{
OcctKnotArrays make_clamped_knots(const std::span<const BSplineKnot> knots, const int degree, const int pole_count)
{
    if (knots.size() < 2 || knots.size() > static_cast<std::size_t>(pole_count))
    {
        throw std::invalid_argument("Invalid number of distinct knots for a clamped B-spline");
    }
    const int count = static_cast<int>(knots.size());
    OcctKnotArrays result{.values = NCollection_Array1<double>{1, count},
                          .multiplicities = NCollection_Array1<int>{1, count}};
    std::int64_t total = 0;
    for (std::size_t i = 0; i < knots.size(); ++i)
    {
        const auto [value, multiplicity] = knots[i];
        if (!std::isfinite(value) || (i > 0 && value <= knots[i - 1].value))
        {
            throw std::invalid_argument("Distinct knots must be finite and strictly increasing");
        }
        const bool endpoint = i == 0 || i + 1 == knots.size();
        if (endpoint ? multiplicity != degree + 1 : (multiplicity < 1 || multiplicity > degree))
        {
            throw std::invalid_argument("Invalid knot multiplicity for this degree of clamped B-spline");
        }
        result.values(static_cast<int>(i) + 1) = value;
        result.multiplicities(static_cast<int>(i) + 1) = multiplicity;
        total += multiplicity;
    }
    if (total - degree - 1 != pole_count)
    {
        throw std::invalid_argument("Knot multiplicities do not match the number of poles");
    }
    return result;
}
} // namespace occt_lab::geometry
