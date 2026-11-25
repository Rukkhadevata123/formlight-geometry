module;

#include <imgui.h>

module formlight_geometry.ui.surface.nurbs_pole_inspector;

import std;
import formlight_geometry.application.editable_nurbs_surface;
import formlight_geometry.document.nurbs_surface_project;

void NurbsPoleInspector::draw_project(EditableNurbsSurface &model)
{
    if (!ImGui::CollapsingHeader("Surface project"))
    {
        return;
    }
    const auto save = [&](const std::filesystem::path &path)
    { return formlight_geometry::document::save_nurbs_surface_project(path, model); };
    const auto open = [&](const std::filesystem::path &path) -> std::expected<void, std::string>
    {
        auto project = formlight_geometry::document::open_nurbs_surface_project(path);
        if (!project)
        {
            return std::unexpected(project.error());
        }
        reset_draft(*project);
        selected_pole_ = 0;
        model = std::move(*project);
        return {};
    };
    project_file_.draw(!tracking_pole_ && has_edits(model.current().definition), save, open);
}
