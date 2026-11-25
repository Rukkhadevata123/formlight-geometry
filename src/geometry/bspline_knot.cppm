export module formlight_geometry.geometry.bspline_knot;

import std;

namespace formlight_geometry::geometry
{
export struct BSplineKnot
{
    double value;
    int multiplicity;
    bool operator==(const BSplineKnot &) const = default;
};

export [[nodiscard]] std::vector<BSplineKnot> make_uniform_clamped_knots(const int pole_count, const int degree)
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

export [[nodiscard]] auto validate_clamped_knots(const std::span<const BSplineKnot> knots, const int degree,
                                                 const int pole_count) -> std::expected<void, std::string>
{
    if (degree < 1 || pole_count <= degree || knots.size() < 2 || knots.size() > static_cast<std::size_t>(pole_count))
    {
        return std::unexpected("Invalid degree, pole count or number of distinct knots for a clamped B-spline");
    }
    std::int64_t total = 0;
    for (std::size_t i = 0; i < knots.size(); ++i)
    {
        const auto [value, multiplicity] = knots[i];
        if (!std::isfinite(value) || (i > 0 && value <= knots[i - 1].value))
        {
            return std::unexpected(std::format("Knot {}: values must be finite and strictly increasing", i + 1));
        }
        const bool endpoint = i == 0 || i + 1 == knots.size();
        if (endpoint && multiplicity != degree + 1)
        {
            return std::unexpected(std::format("Knot {}: endpoint multiplicity must be {}", i + 1, degree + 1));
        }
        if (!endpoint && (multiplicity < 1 || multiplicity > degree))
        {
            return std::unexpected(
                std::format("Knot {}: interior multiplicity must be between 1 and {}", i + 1, degree));
        }
        total += multiplicity;
    }
    const auto required = static_cast<std::int64_t>(pole_count) + degree + 1;
    if (total != required)
    {
        return std::unexpected(std::format("Multiplicity total is {}; expected {}", total, required));
    }
    return {};
}
} // namespace formlight_geometry::geometry
