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
    static constexpr std::size_t side_length = 4;
    using ControlNet = std::array<Vec3, side_length * side_length>;

    // The control points are stored row-major, with u along columns and v along rows.
    [[nodiscard]] static std::expected<CubicBezierPatch, PatchError> create(std::span<const Vec3> control_points);
    [[nodiscard]] std::expected<SurfaceSample, PatchError> evaluate(double u, double v) const;

  private:
    explicit CubicBezierPatch(const ControlNet &control_points);
    ControlNet control_points_;
};

struct PatchIsolines
{
    using Line = std::array<Vec3, 33>;
    using Family = std::array<Line, 9>;
    Family constant_u;
    Family constant_v;
};

[[nodiscard]] auto sample_patch_isolines(const CubicBezierPatch &patch) -> std::expected<PatchIsolines, PatchError>;
} // namespace occt_lab::geometry
