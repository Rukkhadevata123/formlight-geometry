module;

#include <NCollection_Array1.hxx>

export module occt_lab.geometry.occt_bspline_knots;

import std;
import occt_lab.geometry.bspline_knot;

namespace occt_lab::geometry
{
export struct OcctKnotArrays
{
    NCollection_Array1<double> values;
    NCollection_Array1<int> multiplicities;
};

// The caller has checked the degree and pole count against the OCCT limits.
export [[nodiscard]] OcctKnotArrays make_clamped_knots(const std::span<const BSplineKnot> knots, const int degree,
                                                       const int pole_count)
{
    if (const auto valid = validate_clamped_knots(knots, degree, pole_count); !valid)
    {
        throw std::invalid_argument(valid.error());
    }
    const int count = static_cast<int>(knots.size());
    OcctKnotArrays result{.values = NCollection_Array1<double>{1, count},
                          .multiplicities = NCollection_Array1<int>{1, count}};
    for (std::size_t i = 0; i < knots.size(); ++i)
    {
        const auto [value, multiplicity] = knots[i];
        result.values(static_cast<int>(i) + 1) = value;
        result.multiplicities(static_cast<int>(i) + 1) = multiplicity;
    }
    return result;
}
} // namespace occt_lab::geometry
