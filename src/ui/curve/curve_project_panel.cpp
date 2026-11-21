#include "curve_editor.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <filesystem>
#include <utility>

void CurveEditor::draw_project(EditableCurve &curve)
{
    if (!ImGui::CollapsingHeader("Curve project"))
    {
        return;
    }
    ImGui::InputText("Curve name", &settings_.name);
    const auto save = [&](const std::filesystem::path &path)
    { return occt_lab::document::save_curve_project(path, curve.current(), settings_); };
    const auto open = [&](const std::filesystem::path &path) -> std::expected<void, std::string>
    {
        auto project = occt_lab::document::open_curve_project(path);
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
