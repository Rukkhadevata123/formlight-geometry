#include "curve_editor.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <exception>
#include <filesystem>
#include <utility>

void CurveEditor::draw_project(EditableCurve &curve)
{
    if (!ImGui::CollapsingHeader("Curve project"))
    {
        return;
    }
    ImGui::InputText("Curve name", &settings_.name);
    if (ImGui::InputText("Project path", &project_path_))
    {
        project_status_.clear();
    }
    const bool dirty = draft_ != curve.current().definition;
    ImGui::BeginDisabled(dirty || project_path_.empty());
    const bool save = ImGui::Button("Save project");
    ImGui::SameLine();
    const bool open = ImGui::Button("Open project");
    ImGui::EndDisabled();
    if (dirty && !tracking_pole_)
    {
        ImGui::TextUnformatted("Apply or cancel pole edits before saving or opening.");
    }
    if (save || open)
    {
        try
        {
            const auto utf8 = std::u8string(project_path_.begin(), project_path_.end());
            const auto path = std::filesystem::absolute(std::filesystem::path{utf8});
            const auto resolved = path.u8string();
            const std::string location(resolved.begin(), resolved.end());
            if (save)
            {
                const auto result = occt_lab::document::save_curve_project(path, curve.current(), settings_);
                project_status_ = result ? "Saved " + location : result.error();
            }
            else if (auto project = occt_lab::document::open_curve_project(path))
            {
                auto draft = project->curve.definition;
                curve = EditableCurve{std::move(project->curve)};
                draft_ = std::move(draft);
                settings_ = std::move(project->settings);
                error_.clear();
                project_status_ = "Opened " + location;
            }
            else
            {
                project_status_ = project.error();
            }
        }
        catch (const std::exception &error)
        {
            project_status_ = error.what();
        }
    }
    if (!project_status_.empty())
    {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 24.0F * ImGui::GetFontSize());
        ImGui::TextUnformatted(project_status_.c_str());
        ImGui::PopTextWrapPos();
    }
}
