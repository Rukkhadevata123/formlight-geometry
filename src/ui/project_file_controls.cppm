module;

#include <imgui.h>
#include <imgui_stdlib.h>

export module occt_lab.ui.project_file_controls;

import std;

export struct ProjectFileControls
{
    using Operation = std::function<std::expected<void, std::string>(const std::filesystem::path &)>;

    std::string path;
    std::string status;

    void draw(bool pending_edits, const Operation &save, const Operation &open);
};

void ProjectFileControls::draw(const bool pending_edits, const Operation &save, const Operation &open)
{
    if (ImGui::InputText("Project path", &path))
    {
        status.clear();
    }
    ImGui::BeginDisabled(pending_edits || path.empty());
    const bool saving = ImGui::Button("Save project");
    ImGui::SameLine();
    const bool opening = ImGui::Button("Open project");
    ImGui::EndDisabled();
    if (pending_edits)
    {
        ImGui::TextUnformatted("Apply or cancel pole edits before saving or opening.");
    }

    if (saving || opening)
    {
        try
        {
            const std::u8string entered(path.begin(), path.end());
            const auto absolute = std::filesystem::absolute(std::filesystem::path{entered});
            const auto utf8 = absolute.u8string();
            const std::string location(utf8.begin(), utf8.end());
            const auto result = saving ? save(absolute) : open(absolute);
            const auto action = saving ? "Saved " : "Opened ";
            status = result ? action + location : result.error();
        }
        catch (const std::exception &error)
        {
            status = error.what();
        }
    }
    if (!status.empty())
    {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 24.0F * ImGui::GetFontSize());
        ImGui::TextUnformatted(status.c_str());
        ImGui::PopTextWrapPos();
    }
}
