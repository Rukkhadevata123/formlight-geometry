#pragma once

#include "application/editable_nurbs_surface.hpp"
#include "application/nurbs_surface_creation.hpp"
#include "ui/project_file_controls.hpp"
#include "surface_grid_controls.hpp"

namespace occt_lab::render
{
class ViewportProjection;
}

struct ClipTransform;

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
        return net_preview_ ? net_selected_ : selected_pole_;
    }
    [[nodiscard]] bool picking_net() const
    {
        return picked_net_.has_value();
    }
    void cancel_creation(EditableNurbsSurface &model);
    void draw_net_picker(const ClipTransform &transform, bool scene_click);
    [[nodiscard]] const occt_lab::geometry::SurfaceProbe *draw(EditableNurbsSurface &model, NurbsMeshControls *mesh);

  private:
    void draw_creation_dialog(EditableNurbsSurface &model);
    bool draw_net_table();
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
    std::expected<occt_lab::geometry::SurfaceSample, occt_lab::geometry::PatchError> bernstein_sample_;
    occt_lab::SurfaceCreationParameters creation_parameters_;
    occt_lab::SurfaceCreationParameters net_parameters_{.v_poles = 3};
    occt_lab::SurfacePoleRows creation_net_;
    struct NetPicking
    {
        EditableNurbsSurface::Framing framing;
        std::vector<occt_lab::geometry::Vec3> positions;
    };
    std::optional<NetPicking> picked_net_;
    std::optional<EditableNurbsSurface::Framing> net_frame_;
    std::size_t net_selected_ = 0;
    bool resume_net_ = false;
    bool net_preview_ = false;
    int creation_preset_ = 0;
    std::string creation_error_;
    ProjectFileControls project_file_{.path = "surface.occtlab"};
};
