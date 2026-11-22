#pragma once

#include "patch_grid.hpp"
#include "surface_isolines.hpp"

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
    InvalidSampling,
    NonFiniteEvaluation,
    NotPolynomialBicubic,
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

[[nodiscard]] auto sample_patch_isolines(const CubicBezierPatch &patch, PatchSampling sampling = {})
    -> std::expected<PatchIsolines, PatchError>;

struct BSplineSurfaceDefinition;
// Evaluate a single polynomial bicubic span at parameters in its original knot domain.
[[nodiscard]] auto evaluate_bernstein_reference(const BSplineSurfaceDefinition &definition, double u, double v)
    -> std::expected<SurfaceSample, PatchError>;
} // namespace occt_lab::geometry
