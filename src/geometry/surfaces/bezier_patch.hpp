#pragma once

#include "surface_isolines.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
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
    InvalidSampling,
    NonFiniteEvaluation,
};

class CubicBezierPatch
{
  public:
    static constexpr std::size_t side_length = 4;
    static constexpr double normal_tolerance = 1e-12;
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

struct PatchGrid
{
    static constexpr uint32_t max_segments = 32;
    uint32_t u_segments = 24;
    uint32_t v_segments = 16;
    bool operator==(const PatchGrid &) const = default;
};

[[nodiscard]] auto sample_patch_isolines(const CubicBezierPatch &patch, PatchSampling sampling = {})
    -> std::expected<PatchIsolines, PatchError>;
} // namespace occt_lab::geometry
