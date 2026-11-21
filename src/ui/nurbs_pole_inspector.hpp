#pragma once

#include "application/editable_nurbs_surface.hpp"

class NurbsPoleInspector
{
  public:
    explicit NurbsPoleInspector(const EditableNurbsSurface &model);
    void draw(EditableNurbsSurface &model, std::size_t selected_pole);

  private:
    void reset_draft(const EditableNurbsSurface &model);
    std::vector<occt_lab::geometry::Vec3> poles_;
    std::vector<double> weights_;
    std::string error_;
};
