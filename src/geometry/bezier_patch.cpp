#include "bezier_patch.hpp"

#include <algorithm>
#include <cmath>

namespace occt_lab::geometry
{
namespace
{
constexpr Vec3 operator+(const Vec3 &left, const Vec3 &right)
{
    return {.x = left.x + right.x, .y = left.y + right.y, .z = left.z + right.z};
}

constexpr Vec3 operator*(const Vec3 &value, const double scale)
{
    return {.x = value.x * scale, .y = value.y * scale, .z = value.z * scale};
}

constexpr Vec3 cross(const Vec3 &left, const Vec3 &right)
{
    return {
        .x = left.y * right.z - left.z * right.y,
        .y = left.z * right.x - left.x * right.z,
        .z = left.x * right.y - left.y * right.x,
    };
}

double length(const Vec3 &value)
{
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

struct CubicBasis
{
    std::array<double, 4> value;
    std::array<double, 4> derivative;
};

CubicBasis cubic_basis(const double parameter)
{
    const double inverse = 1.0 - parameter;
    return {
        .value =
            {
                inverse * inverse * inverse,
                3.0 * parameter * inverse * inverse,
                3.0 * parameter * parameter * inverse,
                parameter * parameter * parameter,
            },
        .derivative =
            {
                -3.0 * inverse * inverse,
                3.0 * inverse * inverse - 6.0 * parameter * inverse,
                6.0 * parameter * inverse - 3.0 * parameter * parameter,
                3.0 * parameter * parameter,
            },
    };
}
} // namespace

std::expected<CubicBezierPatch, PatchError> CubicBezierPatch::create(const std::span<const Vec3> control_points)
{
    if (control_points.size() != control_point_count)
    {
        return std::unexpected(PatchError::WrongControlPointCount);
    }

    const bool has_invalid_point =
        std::ranges::any_of(control_points, [](const Vec3 &point)
                            { return !std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z); });
    if (has_invalid_point)
    {
        return std::unexpected(PatchError::NonFiniteControlPoint);
    }

    std::array<Vec3, control_point_count> copied_points{};
    std::ranges::copy(control_points, copied_points.begin());
    return CubicBezierPatch{copied_points};
}

std::expected<SurfaceSample, PatchError> CubicBezierPatch::evaluate(const double u, const double v) const
{
    if (!std::isfinite(u) || !std::isfinite(v))
    {
        return std::unexpected(PatchError::NonFiniteParameter);
    }
    if (u < 0.0 || u > 1.0 || v < 0.0 || v > 1.0)
    {
        return std::unexpected(PatchError::ParameterOutOfRange);
    }

    const auto [value_u, derivative_u] = cubic_basis(u);
    const auto [value_v, derivative_v] = cubic_basis(v);
    Vec3 position{};
    Vec3 tangent_u{};
    Vec3 tangent_v{};

    for (std::size_t row = 0; row < side_length; ++row)
    {
        for (std::size_t column = 0; column < side_length; ++column)
        {
            const Vec3 &control_point = control_points_[row * side_length + column];
            position = position + control_point * (value_u[column] * value_v[row]);
            tangent_u = tangent_u + control_point * (derivative_u[column] * value_v[row]);
            tangent_v = tangent_v + control_point * (value_u[column] * derivative_v[row]);
        }
    }

    const Vec3 unnormalized_normal = cross(tangent_u, tangent_v);
    const double normal_length = length(unnormalized_normal);
    if (constexpr double normal_tolerance = 1e-12; normal_length <= normal_tolerance)
    {
        return std::unexpected(PatchError::DegenerateNormal);
    }

    return SurfaceSample{
        .position = position,
        .normal = unnormalized_normal * (1.0 / normal_length),
    };
}

CubicBezierPatch::CubicBezierPatch(const std::array<Vec3, control_point_count> &control_points)
    : control_points_(control_points)
{
}
} // namespace occt_lab::geometry
