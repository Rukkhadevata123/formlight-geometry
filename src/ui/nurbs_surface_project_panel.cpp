#include "nurbs_pole_inspector.hpp"

#include "document/nurbs_surface_project.hpp"

#include <imgui.h>

#include <utility>

void NurbsPoleInspector::draw_project(EditableNurbsSurface &model)
{
    const bool expanded = ImGui::CollapsingHeader("Surface project");
    ImGui::SetItemTooltip("Open a NURBS surface or Bezier patch. Choose a new path to save a converted copy.");
    if (!expanded)
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
