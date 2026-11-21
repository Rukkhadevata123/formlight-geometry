#pragma once

#include "application/editable_nurbs_surface.hpp"
#include "application/nurbs_surface_creation.hpp"
#include "ui/project_file_controls.hpp"
#include "surface_grid_controls.hpp"

namespace occt_lab::render
{
class ViewportProjection;
}

class NurbsPoleInspector
{
  public:
    explicit NurbsPoleInspector(const EditableNurbsSurface &model);
    void handle_viewport_input(EditableNurbsSurface &model, const occt_lab::render::ViewportProjection &project,
                               bool pole_click);
    void cancel_drag(EditableNurbsSurface &model);
    [[nodiscard]] bool tracking_pole() const
    {
        return tracking_pole_;
    }
    [[nodiscard]] std::size_t selected_pole() const
    {
        return selected_pole_;
    }
    [[nodiscard]] const occt_lab::geometry::SurfaceProbe *draw(EditableNurbsSurface &model,
                                                               NurbsComparisonControls *comparison);

  private:
    void draw_creation_dialog(EditableNurbsSurface &model);
    void draw_project(EditableNurbsSurface &model);
    const occt_lab::geometry::SurfaceProbe *update_probe(const EditableNurbsSurface::Snapshot &snapshot,
                                                         bool show_controls);
    void reset_draft(const EditableNurbsSurface &model);
    [[nodiscard]] bool has_edits(const EditableNurbsSurface::Definition &surface) const;
    std::size_t selected_pole_ = 0;
    int draft_columns_ = 0;
    bool tracking_pole_ = false;
    std::vector<occt_lab::geometry::Vec3> poles_;
    std::vector<double> weights_;
    std::string error_;
    std::string sampling_error_;
    std::shared_ptr<const occt_lab::geometry::BSplineSurface> probed_surface_;
    double probe_u_ = 0.0;
    double probe_v_ = 0.0;
    std::expected<occt_lab::geometry::SurfaceProbe, std::string> probe_;
    occt_lab::PlanarSurfaceParameters creation_parameters_;
    int creation_preset_ = 0;
    std::string creation_error_;
    ProjectFileControls project_file_{.path = "surface.occtlab"};
};
