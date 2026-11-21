#pragma once

#include "application/editable_nurbs_surface.hpp"
#include "application/nurbs_surface_creation.hpp"

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
    void draw(EditableNurbsSurface &model);

  private:
    void draw_creation_dialog(EditableNurbsSurface &model);
    void reset_draft(const EditableNurbsSurface &model);
    [[nodiscard]] bool has_edits(const EditableNurbsSurface::Definition &surface) const;
    std::size_t selected_pole_ = 0;
    int draft_columns_ = 0;
    bool tracking_pole_ = false;
    std::vector<occt_lab::geometry::Vec3> poles_;
    std::vector<double> weights_;
    std::string error_;
    occt_lab::PlanarSurfaceParameters creation_parameters_;
    std::string creation_error_;
};
