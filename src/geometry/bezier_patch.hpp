#pragma once

#include "vec3.hpp"

#include <array>
#include <cstddef>
#include <expected>
#include <span>
#include <vector>

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
    InvalidSampling,
};

class CubicBezierPatch
{
  public:
    static constexpr std::size_t side_length = 4;
    using ControlNet = std::array<Vec3, side_length * side_length>;

    // The control points are stored row-major, with u along columns and v along rows.
    [[nodiscard]] static std::expected<CubicBezierPatch, PatchError> create(std::span<const Vec3> control_points);
    [[nodiscard]] std::expected<SurfaceSample, PatchError> evaluate(double u, double v) const;
    [[nodiscard]] auto control_net() const -> const ControlNet &
    {
        return control_points_;
    }

  private:
    explicit CubicBezierPatch(const ControlNet &control_points);
    ControlNet control_points_;
};

struct PatchSampling
{
    static constexpr int minimum_count = 2;
    static constexpr int max_lines = 33;
    static constexpr int max_samples = 129;

    int constant_u_lines = 9;
    int constant_v_lines = 9;
    int samples_per_line = 33;
    bool operator==(const PatchSampling &) const = default;
};

struct PatchIsolines
{
    using Line = std::vector<Vec3>;
    using Family = std::vector<Line>;
    Family constant_u;
    Family constant_v;
};

[[nodiscard]] auto sample_patch_isolines(const CubicBezierPatch &patch, PatchSampling sampling = {})
    -> std::expected<PatchIsolines, PatchError>;
} // namespace occt_lab::geometry
