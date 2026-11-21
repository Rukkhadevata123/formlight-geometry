#pragma once

#include "geometry/bezier_patch.hpp"

#include <string>

class EditablePatch;

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
    explicit BezierPatchInspector(EditablePatch &model);
    void draw(PatchMeshControls &mesh, bool compute_supported);
    [[nodiscard]] auto selected_sample() const -> const occt_lab::geometry::SurfaceSample *
    {
        return sample_ ? &*sample_ : nullptr;
    }

  private:
    void update_samples();
    void draw_patch_commands(PatchMeshControls &mesh);
    void draw_reference() const;
    void draw_sampling_controls();
    EditablePatch &model_;
    std::string edit_error_;
    bool sampling_failed_ = false;
    double u_ = 0.5;
    double v_ = 0.5;
    std::expected<occt_lab::geometry::SurfaceSample, occt_lab::geometry::PatchError> sample_;
    std::expected<occt_lab::geometry::SurfaceSample, std::string> reference_sample_;
};
