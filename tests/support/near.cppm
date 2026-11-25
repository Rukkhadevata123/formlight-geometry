export module formlight_geometry.test.support.near;

import std;
import formlight_geometry.geometry.vec3;

export namespace formlight_geometry::test
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
} // namespace formlight_geometry::test
