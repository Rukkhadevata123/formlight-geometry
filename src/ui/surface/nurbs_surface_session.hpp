#pragma once

#include "application/editable_nurbs_surface.hpp"
#include "nurbs_pole_inspector.hpp"

struct ClipTransform;
struct PatchLayers;

class NurbsSurfaceSession
{
  public:
    NurbsSurfaceSession();
    [[nodiscard]] bool tracking_pole() const
    {
        return inspector_.tracking_pole();
    }
    void cancel_drag();
    [[nodiscard]] bool picking_net() const
    {
        return inspector_.picking_net();
    }
    void cancel_creation()
    {
        inspector_.cancel_creation(model_);
    }
    [[nodiscard]] const EditableNurbsSurface::Snapshot &draw(const ClipTransform &transform, const PatchLayers &layers,
                                                             bool pole_click, NurbsMeshControls *mesh);

  private:
    EditableNurbsSurface model_;
    NurbsPoleInspector inspector_;
};
