export module occt_lab.ui.surface.nurbs_surface_session;

import occt_lab.application.editable_nurbs_surface;
import occt_lab.application.nurbs_surface_creation;
import occt_lab.render.preview.surface.nurbs_surface_preview;
import occt_lab.render.preview.surface.surface_wireframe;
import occt_lab.render.viewport.clip_transform;
import occt_lab.ui.surface.nurbs_pole_inspector;
import occt_lab.ui.surface.surface_grid_controls;

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

NurbsSurfaceSession::NurbsSurfaceSession() : model_(occt_lab::make_saddle_surface()), inspector_(model_)
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
        draw_nurbs_surface_preview(model_.displayed(), transform, layers, inspector_.selected_pole(), probe);
    }
    return model_.displayed();
}
