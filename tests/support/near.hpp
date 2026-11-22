#pragma once

#include "geometry/vec3.hpp"

#include <cmath>

namespace occt_lab::test
{
inline bool near(double actual, double expected, double tolerance = 1e-12)
{
    return std::abs(actual - expected) <= tolerance;
}

inline bool near(const geometry::Vec3 &actual, const geometry::Vec3 &expected)
{
    constexpr double tolerance = 1e-12;
    return std::hypot(actual.x - expected.x, actual.y - expected.y, actual.z - expected.z) <= tolerance;
}
} // namespace occt_lab::test
