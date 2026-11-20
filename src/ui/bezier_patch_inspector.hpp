#pragma once

#include "geometry/bezier_patch.hpp"

class BezierPatchPreview;

class BezierPatchInspector
{
  public:
    explicit BezierPatchInspector(BezierPatchPreview &preview);
    void draw();
    [[nodiscard]] auto selected_sample() const -> const occt_lab::geometry::SurfaceSample *
    {
        return sample_ ? &*sample_ : nullptr;
    }

  private:
    void draw_sampling_controls();
    BezierPatchPreview &preview_;
    bool sampling_failed_ = false;
    double u_ = 0.5;
    double v_ = 0.5;
    std::expected<occt_lab::geometry::SurfaceSample, occt_lab::geometry::PatchError> sample_;
};
