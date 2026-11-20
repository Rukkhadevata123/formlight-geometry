#pragma once

#include "geometry/bezier_patch.hpp"

class BezierPatchInspector
{
  public:
    explicit BezierPatchInspector(const occt_lab::geometry::CubicBezierPatch &patch);
    void draw();
    [[nodiscard]] auto selected_sample() const -> const occt_lab::geometry::SurfaceSample *
    {
        return sample_ ? &*sample_ : nullptr;
    }

  private:
    const occt_lab::geometry::CubicBezierPatch &patch_;
    double u_ = 0.5;
    double v_ = 0.5;
    std::expected<occt_lab::geometry::SurfaceSample, occt_lab::geometry::PatchError> sample_;
};
