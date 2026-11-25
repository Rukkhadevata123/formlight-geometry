export module formlight_geometry.ui.surface.nurbs_surface_session;

import formlight_geometry.application.editable_nurbs_surface;
import formlight_geometry.application.nurbs_surface_creation;
import formlight_geometry.render.preview.surface.nurbs_surface_preview;
import formlight_geometry.render.preview.surface.surface_wireframe;
import formlight_geometry.render.viewport.clip_transform;
import formlight_geometry.ui.surface.nurbs_pole_inspector;
import formlight_geometry.ui.surface.surface_grid_controls;

export class NurbsSurfaceSession
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

NurbsSurfaceSession::NurbsSurfaceSession() : model_(formlight_geometry::make_saddle_surface()), inspector_(model_)
{
}

void NurbsSurfaceSession::cancel_drag()
{
    inspector_.cancel_drag(model_);
}

const EditableNurbsSurface::Snapshot &NurbsSurfaceSession::draw(const ClipTransform &transform, const PatchLayers &layers,
                                                               const bool pole_click, NurbsMeshControls *mesh)
{
    if (inspector_.picking_net())
    {
        if (mesh)
        {
            mesh->requested = false;
        }
        inspector_.draw_net_picker(transform, pole_click);
        return model_.current();
    }
    if (layers.control_net)
    {
        const auto project = nurbs_surface_projection(transform, model_.current().framing);
        inspector_.handle_viewport_input(model_, project, pole_click);
    }
    else
    {
        cancel_drag();
    }
    const auto *probe = inspector_.draw(model_, mesh);
    if (!inspector_.picking_net())
    {
        const auto &snapshot = model_.displayed();
        draw_nurbs_surface_preview(snapshot, transform, layers, inspector_.selected_point(snapshot.definition), probe);
    }
    return model_.displayed();
}
