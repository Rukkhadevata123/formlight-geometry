#pragma once

#include "geometry/bezier_patch.hpp"

#include <string>

class EditablePatch;
struct ClipTransform;

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
    void handle_viewport_input(const ClipTransform &transform, bool pole_click, PatchMeshControls &mesh);
    void cancel_drag();
    [[nodiscard]] bool tracking_pole() const
    {
        return tracking_pole_;
    }
    [[nodiscard]] std::size_t selected_pole() const;
    [[nodiscard]] auto selected_sample() const -> const occt_lab::geometry::SurfaceSample *
    {
        return sample_ ? &*sample_ : nullptr;
    }

  private:
    void select_pole(std::size_t index);
    void update_samples();
    void refresh_after_edit(PatchMeshControls &mesh);
    [[nodiscard]] bool draw_pole_editor();
    void draw_patch_commands(PatchMeshControls &mesh);
    void draw_reference() const;
    void draw_sampling_controls();
    EditablePatch &model_;
    occt_lab::geometry::CubicBezierPatch::ControlNet draft_;
    int pole_u_ = 1;
    int pole_v_ = 1;
    bool reveal_selected_pole_ = false;
    bool tracking_pole_ = false;
    std::string edit_error_;
    bool sampling_failed_ = false;
    double u_ = 0.5;
    double v_ = 0.5;
    std::expected<occt_lab::geometry::SurfaceSample, occt_lab::geometry::PatchError> sample_;
    std::expected<occt_lab::geometry::SurfaceSample, std::string> reference_sample_;
};
