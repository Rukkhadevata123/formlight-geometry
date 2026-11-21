#pragma once

#include "geometry/occt_bezier_patch.hpp"

class BezierPatchPreview;

struct PatchMeshControls
{
    occt_lab::geometry::PatchGrid grid;
    bool visible = false;
    bool compare_requested = false;
    std::string report;
};

class BezierPatchInspector
{
  public:
    explicit BezierPatchInspector(BezierPatchPreview &preview);
    void draw(PatchMeshControls &mesh, bool compute_supported);
    [[nodiscard]] auto selected_sample() const -> const occt_lab::geometry::SurfaceSample *
    {
        return sample_ ? &*sample_ : nullptr;
    }

  private:
    void update_samples();
    void draw_reference() const;
    void draw_sampling_controls();
    BezierPatchPreview &preview_;
    occt_lab::geometry::OcctBezierPatch reference_patch_;
    bool sampling_failed_ = false;
    double u_ = 0.5;
    double v_ = 0.5;
    std::expected<occt_lab::geometry::SurfaceSample, occt_lab::geometry::PatchError> sample_;
    std::expected<occt_lab::geometry::SurfaceSample, std::string> reference_sample_;
};
