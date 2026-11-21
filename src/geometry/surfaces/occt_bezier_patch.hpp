#pragma once

#include "bezier_patch.hpp"

#include <Geom_BezierSurface.hxx>

#include <string>

namespace occt_lab::geometry
{
class OcctBezierPatch
{
  public:
    explicit OcctBezierPatch(const CubicBezierPatch &patch);
    [[nodiscard]] auto evaluate(double u, double v) const -> std::expected<SurfaceSample, std::string>;

  private:
    Geom_BezierSurface surface_;
};

struct SurfaceSampleDifference
{
    double position_distance_mm;
    double normal_angle_degrees;
};

[[nodiscard]] SurfaceSampleDifference compare_surface_samples(const SurfaceSample &sample,
                                                              const SurfaceSample &reference);
} // namespace occt_lab::geometry
