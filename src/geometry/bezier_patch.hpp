#pragma once

#include "vec3.hpp"

#include <array>
#include <cstddef>
#include <expected>
#include <span>

namespace occt_lab::geometry
{
struct SurfaceSample
{
    Vec3 position;
    Vec3 normal;
};

enum class PatchError
{
    WrongControlPointCount,
    NonFiniteControlPoint,
    NonFiniteParameter,
    ParameterOutOfRange,
    DegenerateNormal,
};

class CubicBezierPatch
{
  public:
    // The control points are stored row-major, with u along columns and v along rows.
    [[nodiscard]] static std::expected<CubicBezierPatch, PatchError> create(std::span<const Vec3> control_points);
    [[nodiscard]] std::expected<SurfaceSample, PatchError> evaluate(double u, double v) const;

  private:
    static constexpr std::size_t side_length = 4;
    static constexpr std::size_t control_point_count = side_length * side_length;

    explicit CubicBezierPatch(const std::array<Vec3, control_point_count> &control_points);

    std::array<Vec3, control_point_count> control_points_;
};
} // namespace occt_lab::geometry
