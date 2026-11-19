#include "geometry/bspline_curve.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <print>
#include <stdexcept>

namespace
{
using occt_lab::geometry::sample_clamped_cubic_bspline;
using occt_lab::geometry::Vec3;

bool near(const Vec3 &actual, const Vec3 &expected)
{
    constexpr double tolerance = 1e-12;
    return std::hypot(actual.x - expected.x, actual.y - expected.y, actual.z - expected.z) <= tolerance;
}

void check_curve_samples()
{
    constexpr std::array poles{
        Vec3{.x = -3.0, .y = 1.0, .z = 0.0}, Vec3{.x = -2.0, .y = 2.0, .z = 1.0}, Vec3{.x = 1.0, .y = 4.0, .z = 3.0},
        Vec3{.x = 3.0, .y = -2.0, .z = 1.0}, Vec3{.x = 6.0, .y = 0.0, .z = 0.0},
    };
    std::array<Vec3, 3> samples{};
    sample_clamped_cubic_bspline(poles, samples);

    if (!near(samples.front(), poles.front()) || !near(samples.back(), poles.back()))
    {
        throw std::runtime_error("The clamped curve must interpolate both endpoint poles");
    }
    // At the interior knot, the three active pole weights are 1/4, 1/2 and 1/4.
    if (constexpr Vec3 expected_middle{.x = 0.75, .y = 2.0, .z = 2.0}; !near(samples[1], expected_middle))
    {
        throw std::runtime_error("Unexpected cubic B-spline sample at the middle knot");
    }
}
} // namespace

int main()
{
    try
    {
        check_curve_samples();
        std::println("Cubic B-spline endpoint and interior knot samples verified");
        return EXIT_SUCCESS;
    }
    catch (const std::exception &error)
    {
        std::println(stderr, "bspline_curve_check: {}", error.what());
        return EXIT_FAILURE;
    }
}
