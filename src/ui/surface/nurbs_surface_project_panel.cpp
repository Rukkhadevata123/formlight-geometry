module;

#include <imgui.h>

module occt_lab.ui.surface.nurbs_pole_inspector;

import std;
import occt_lab.application.editable_nurbs_surface;
import occt_lab.document.nurbs_surface_project;

void NurbsPoleInspector::draw_project(EditableNurbsSurface &model)
{
    if (!ImGui::CollapsingHeader("Surface project"))
    {
        return;
    }
    const auto save = [&](const std::filesystem::path &path)
    { return occt_lab::document::save_nurbs_surface_project(path, model); };
    const auto open = [&](const std::filesystem::path &path) -> std::expected<void, std::string>
    {
        auto project = occt_lab::document::open_nurbs_surface_project(path);
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
