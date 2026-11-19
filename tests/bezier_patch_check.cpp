#include "geometry/bezier_patch.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <format>
#include <limits>
#include <print>
#include <stdexcept>

namespace
{
using occt_lab::geometry::CubicBezierPatch;
using occt_lab::geometry::PatchError;
using occt_lab::geometry::Vec3;

constexpr std::size_t side_length = 4;
using ControlNet = std::array<Vec3, side_length * side_length>;

struct ExpectedSample
{
    double u;
    double v;
    Vec3 position;
    Vec3 normal;
};

ControlNet raised_patch()
{
    ControlNet control_points{};
    for (std::size_t row = 0; row < side_length; ++row)
    {
        for (std::size_t column = 0; column < side_length; ++column)
        {
            const bool is_interior = (row == 1 || row == 2) && (column == 1 || column == 2);
            control_points[row * side_length + column] = {
                .x = static_cast<double>(column),
                .y = static_cast<double>(row),
                .z = is_interior ? 0.4 : 0.0,
            };
        }
    }
    return control_points;
}

bool near(const Vec3 &actual, const Vec3 &expected)
{
    constexpr double tolerance = 1e-12;
    return std::hypot(actual.x - expected.x, actual.y - expected.y, actual.z - expected.z) <= tolerance;
}

void check_samples(const CubicBezierPatch &patch)
{
    constexpr Vec3 up{.x = 0.0, .y = 0.0, .z = 1.0};
    // The z slopes relative to x and y at (0.25, 0.75) are +0.1125 and -0.1125.
    constexpr double slope = 0.1125;
    const double normal_length = std::sqrt(1.0 + 2.0 * slope * slope);
    const Vec3 off_center_normal{
        .x = -slope / normal_length,
        .y = slope / normal_length,
        .z = 1.0 / normal_length,
    };
    const std::array samples{
        ExpectedSample{.u = 0.0, .v = 0.0, .position = {.x = 0.0, .y = 0.0, .z = 0.0}, .normal = up},
        ExpectedSample{.u = 0.5, .v = 0.5, .position = {.x = 1.5, .y = 1.5, .z = 0.225}, .normal = up},
        ExpectedSample{.u = 1.0, .v = 1.0, .position = {.x = 3.0, .y = 3.0, .z = 0.0}, .normal = up},
        ExpectedSample{
            .u = 0.25,
            .v = 0.75,
            .position = {.x = 0.75, .y = 2.25, .z = 0.1265625},
            .normal = off_center_normal,
        },
    };

    for (const auto &[u, v, position, normal] : samples)
    {
        const auto sample = patch.evaluate(u, v);
        if (!sample)
        {
            throw std::runtime_error(std::format("Could not evaluate the patch at ({}, {})", u, v));
        }
        if (!near(sample->position, position))
        {
            throw std::runtime_error(std::format("Unexpected position at ({}, {})", u, v));
        }
        if (!near(sample->normal, normal))
        {
            throw std::runtime_error(std::format("Unexpected normal at ({}, {})", u, v));
        }
    }
}

void check_invalid_inputs(const CubicBezierPatch &patch)
{
    if (const auto sample = patch.evaluate(1.25, 0.5); sample || sample.error() != PatchError::ParameterOutOfRange)
    {
        throw std::runtime_error("Expected an out-of-range parameter error");
    }
    if (const auto sample = patch.evaluate(0.5, std::numeric_limits<double>::quiet_NaN());
        sample || sample.error() != PatchError::NonFiniteParameter)
    {
        throw std::runtime_error("Expected a non-finite parameter error");
    }
    if (const auto empty = CubicBezierPatch::create({}); empty || empty.error() != PatchError::WrongControlPointCount)
    {
        throw std::runtime_error("Expected a control point count error");
    }
}
} // namespace

int main()
{
    try
    {
        const auto control_points = raised_patch();
        const auto patch = CubicBezierPatch::create(control_points);
        if (!patch)
        {
            throw std::runtime_error("Could not create the sample Bezier patch");
        }

        check_samples(*patch);
        check_invalid_inputs(*patch);
        std::println("Bezier patch positions, normals and invalid inputs verified");
        return EXIT_SUCCESS;
    }
    catch (const std::exception &error)
    {
        std::println(stderr, "bezier_patch_check: {}", error.what());
        return EXIT_FAILURE;
    }
}
