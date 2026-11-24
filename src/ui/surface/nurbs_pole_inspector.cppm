module;

#include <imgui.h>

export module occt_lab.ui.surface.nurbs_pole_inspector;

import std;
import occt_lab.application.editable_nurbs_surface;
import occt_lab.application.nurbs_surface_creation;
import occt_lab.geometry.surfaces.bezier_patch;
import occt_lab.geometry.surfaces.bspline_surface;
import occt_lab.render.viewport.clip_transform;
import occt_lab.render.viewport.viewport_projection;
import occt_lab.ui.pole_drag;
import occt_lab.ui.project_file_controls;
import occt_lab.ui.surface.isoline_sampling;
import occt_lab.ui.surface.surface_grid_controls;

export class NurbsPoleInspector
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
    [[nodiscard]] bool begin_inspector_window() const;
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

NurbsPoleInspector::NurbsPoleInspector(const EditableNurbsSurface &model)
{
    reset_draft(model);
}

void NurbsPoleInspector::reset_draft(const EditableNurbsSurface &model)
{
    const auto &definition = model.current().definition;
    poles_.reserve(definition.poles.size());
    weights_.reserve(definition.weights.size());
    if (draft_columns_ != definition.u.pole_count || poles_.size() != definition.poles.size())
    {
        selected_pole_ = 0;
    }
    draft_columns_ = definition.u.pole_count;
    poles_ = definition.poles;
    weights_ = definition.weights;
    error_.clear();
    sampling_error_.clear();
}

bool NurbsPoleInspector::has_edits(const EditableNurbsSurface::Definition &surface) const
{
    return poles_ != surface.poles || weights_ != surface.weights;
}

void NurbsPoleInspector::cancel_drag(EditableNurbsSurface &model)
{
    if (tracking_pole_)
    {
        model.discard_preview();
        tracking_pole_ = false;
        reset_draft(model);
    }
}

void NurbsPoleInspector::handle_viewport_input(EditableNurbsSurface &model,
                                               const occt_lab::render::ViewportProjection &project,
                                               const bool pole_click)
{
    const auto &surface = model.current().definition;
    if (!tracking_pole_)
    {
        if (pole_click)
        {
            if (const auto hit = project.pick_pole(surface.poles, ImGui::GetMousePos()))
            {
                selected_pole_ = *hit;
                tracking_pole_ = !has_edits(surface);
            }
        }
        return;
    }
    const auto motion = read_pole_drag();
    if (!motion)
    {
        cancel_drag(model);
        return;
    }
    const auto position = project.dragged_position(surface.poles.at(selected_pole_), motion->delta);
    if (const auto result = model.preview_pole_move(selected_pole_, position); !result)
    {
        cancel_drag(model);
        error_ = result.error();
        return;
    }
    poles_[selected_pole_] = position;
    if (motion->released)
    {
        model.commit_preview();
        tracking_pole_ = false;
        reset_draft(model);
    }
}

bool NurbsPoleInspector::begin_inspector_window() const
{
    const auto *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({viewport->WorkPos.x + viewport->WorkSize.x - 16.0F, viewport->WorkPos.y + 16.0F},
                            ImGuiCond_FirstUseEver, {1.0F, 0.0F});
    // Both titles identify the same ImGui window so picking follows the inspector if it was moved.
    const char *title = picking_net() ? "Pick surface control net###SurfaceInspector"
                                      : "NURBS control point###SurfaceInspector";
    const auto &style = ImGui::GetStyle();
    // Reserve a frame for the collapse button even when the selected pole has short coordinates.
    const float min_width =
        ImGui::CalcTextSize(title, nullptr, true).x + ImGui::GetFrameHeight() + style.ItemInnerSpacing.x +
        2.0F * style.FramePadding.x;
    constexpr float unlimited = std::numeric_limits<float>::max();
    ImGui::SetNextWindowSizeConstraints({min_width, 0.0F}, {unlimited, unlimited});
    return ImGui::Begin(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize);
}

const occt_lab::geometry::SurfaceProbe *NurbsPoleInspector::draw(EditableNurbsSurface &model,
                                                                NurbsMeshControls *mesh)
{
    const bool visible = begin_inspector_window();
    if (visible)
    {
        ImGui::BeginDisabled(tracking_pole_);
        draw_project(model);
        ImGui::EndDisabled();
        const auto &surface = model.current().definition;
        const auto columns = static_cast<std::size_t>(surface.u.pole_count);
        ImGui::Text("Surface degree   U %d   V %d", surface.u.degree, surface.v.degree);
        ImGui::Text("U [%.8g, %.8g]   V [%.8g, %.8g]", surface.u.knots.front().value, surface.u.knots.back().value,
                    surface.v.knots.front().value, surface.v.knots.back().value);
        ImGui::BeginDisabled(tracking_pole_);
        ImGui::PushItemWidth(11.0F * ImGui::GetFontSize());
        int pole_u = static_cast<int>(selected_pole_ % columns) + 1;
        int pole_v = static_cast<int>(selected_pole_ / columns) + 1;
        ImGui::SliderInt("Pole U", &pole_u, 1, surface.u.pole_count, "%d", ImGuiSliderFlags_AlwaysClamp);
        ImGui::SliderInt("Pole V", &pole_v, 1, surface.v.pole_count, "%d", ImGuiSliderFlags_AlwaysClamp);
        selected_pole_ = (pole_v - 1) * columns + (pole_u - 1);
        auto &[x, y, z] = poles_.at(selected_pole_);
        ImGui::PushID(static_cast<int>(selected_pole_));
        ImGui::PushItemFlag(ImGuiItemFlags_LiveEditOnInputScalar, true);
        constexpr auto flags = ImGuiInputTextFlags_ParseEmptyRefVal;
        const bool changed_x = ImGui::InputDouble("X (mm)", &x, 0, 0, "%.8g", flags);
        const bool changed_y = ImGui::InputDouble("Y (mm)", &y, 0, 0, "%.8g", flags);
        const bool changed_z = ImGui::InputDouble("Z (mm)", &z, 0, 0, "%.8g", flags);
        const bool changed_weight = ImGui::InputDouble("Weight", &weights_.at(selected_pole_), 0, 0, "%.8g", flags);
        ImGui::PopItemFlag();
        ImGui::PopItemWidth();
        ImGui::PopID();
        if (changed_x || changed_y || changed_z || changed_weight)
        {
            error_.clear();
        }

        bool dirty = has_edits(surface);
        ImGui::BeginDisabled(!dirty);
        if (ImGui::Button("Apply"))
        {
            const auto result = model.apply(poles_, weights_);
            if (result)
            {
                dirty = false;
                error_.clear();
            }
            else
            {
                error_ = result.error();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            reset_draft(model);
            dirty = false;
        }
        ImGui::EndDisabled();

        ImGui::BeginDisabled(dirty || !model.can_undo());
        if (ImGui::Button("Undo"))
        {
            model.undo();
            reset_draft(model);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(dirty || !model.can_redo());
        if (ImGui::Button("Redo"))
        {
            model.redo();
            reset_draft(model);
        }
        ImGui::EndDisabled();
        ImGui::BeginDisabled(dirty);
        draw_creation_dialog(model);
        ImGui::EndDisabled();
        ImGui::EndDisabled();
        const auto &point = surface.poles.at(selected_pole_);
        ImGui::Text("Applied (mm)   (%.8g, %.8g, %.8g)", point.x, point.y, point.z);
        ImGui::Text("Applied weight   %.8g", surface.weights.at(selected_pole_));
        if (!error_.empty())
        {
            ImGui::TextWrapped("%s", error_.c_str());
        }
        ImGui::BeginDisabled(tracking_pole_);
        auto sampling = model.sampling();
        if (draw_isoline_sampling_controls(sampling))
        {
            const auto result = model.resample(sampling);
            sampling_error_ = result ? std::string{} : result.error();
        }
        ImGui::EndDisabled();
        if (!sampling_error_.empty())
        {
            ImGui::TextWrapped("%s", sampling_error_.c_str());
        }
    }
    if (mesh)
    {
        mesh->requested = false;
        if (probed_surface_ != model.displayed().surface)
        {
            mesh->report.clear();
        }
        if (visible && ImGui::CollapsingHeader("GPU surface mesh"))
        {
            ImGui::Checkbox("Show GPU mesh", &mesh->visible);
            if (draw_grid_subdivisions(mesh->grid))
            {
                mesh->report.clear();
            }
            ImGui::BeginDisabled(tracking_pole_ || has_edits(model.current().definition));
            mesh->requested = ImGui::Button("Compare positions and normals with OCCT");
            ImGui::EndDisabled();
            if (!mesh->report.empty())
            {
                ImGui::TextUnformatted(mesh->report.c_str());
            }
        }
    }
    const auto *probe = update_probe(
        model.displayed(), visible && ImGui::CollapsingHeader("Surface probe", ImGuiTreeNodeFlags_DefaultOpen));
    ImGui::End();
    return probe;
}
