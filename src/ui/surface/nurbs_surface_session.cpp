#include "nurbs_surface_session.hpp"

#include "application/nurbs_surface_creation.hpp"
#include "render/preview/surface/nurbs_surface_preview.hpp"
#include "render/preview/surface/surface_wireframe.hpp"

NurbsSurfaceSession::NurbsSurfaceSession() : model_(occt_lab::make_saddle_surface()), inspector_(model_)
{
}

void NurbsSurfaceSession::cancel_drag()
{
    inspector_.cancel_drag(model_);
}

const EditableNurbsSurface::Snapshot &NurbsSurfaceSession::draw(const ClipTransform &transform, const PatchLayers &layers,
                                                               const bool pole_click, NurbsComparisonControls *comparison)
{
    if (layers.control_net)
    {
        const auto project = nurbs_surface_projection(transform, model_.current().framing);
        inspector_.handle_viewport_input(model_, project, pole_click);
    }
    else
    {
        cancel_drag();
    }
    const auto *probe = inspector_.draw(model_, comparison);
    draw_nurbs_surface_preview(model_.displayed(), transform, layers, inspector_.selected_pole(), probe);
    return model_.displayed();
}
