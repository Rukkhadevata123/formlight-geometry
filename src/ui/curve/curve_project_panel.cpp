module;

#include <imgui.h>
#include <imgui_stdlib.h>

module formlight_geometry.ui.curve.curve_editor;

import std;
import formlight_geometry.application.editable_curve;
import formlight_geometry.document.curve_project;

void CurveEditor::draw_project(EditableCurve &curve)
{
    if (!ImGui::CollapsingHeader("Curve project"))
    {
        return;
    }
    ImGui::InputText("Curve name", &settings_.name);
    const auto save = [&](const std::filesystem::path &path)
    { return formlight_geometry::document::save_curve_project(path, curve.current(), settings_); };
    const auto open = [&](const std::filesystem::path &path) -> std::expected<void, std::string>
    {
        auto project = formlight_geometry::document::open_curve_project(path);
        if (!project)
        {
            return std::unexpected(project.error());
        }
        auto draft = project->curve.definition;
        curve = EditableCurve{std::move(project->curve)};
        draft_ = std::move(draft);
        settings_ = std::move(project->settings);
        error_.clear();
        return {};
    };
    project_file_.draw(!tracking_pole_ && draft_ != curve.current().definition, save, open);
}
