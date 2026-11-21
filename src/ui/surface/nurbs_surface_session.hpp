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
    [[nodiscard]] const EditableNurbsSurface::Snapshot &draw(const ClipTransform &transform, const PatchLayers &layers,
                                                             bool pole_click, NurbsComparisonControls *comparison);

  private:
    EditableNurbsSurface model_;
    NurbsPoleInspector inspector_;
};
