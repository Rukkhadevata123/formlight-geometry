module;

#include <imgui.h>

export module occt_lab.ui.box_editor;

import std;
import occt_lab.geometry.brep.box_model;
import occt_lab.render.preview.brep_preview;

export class BoxEditor
{
  public:
    explicit BoxEditor(const BoxDimensions &dimensions) : draft_(dimensions)
    {
    }
    void draw(BRepPreview &preview);

  private:
    void draw_history(BRepPreview &preview);
    BoxDimensions draft_;
    std::string error_;
};

void BoxEditor::draw(BRepPreview &preview)
{
    const auto *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({viewport->WorkPos.x + viewport->WorkSize.x - 16.0F, viewport->WorkPos.y + 16.0F},
                            ImGuiCond_FirstUseEver, {1.0F, 0.0F});
    if (ImGui::Begin("Box dimensions (mm)", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        constexpr auto input_flags = ImGuiInputTextFlags_ParseEmptyRefVal;
        // Update the draft while typing so the actions reflect edits before the field loses focus.
        ImGui::PushItemFlag(ImGuiItemFlags_LiveEditOnInputScalar, true);
        ImGui::PushItemWidth(8.0F * ImGui::GetFontSize());
        const bool length_changed = ImGui::InputDouble("Length", &draft_.length, 0.0, 0.0, "%.8g", input_flags);
        const bool width_changed = ImGui::InputDouble("Width", &draft_.width, 0.0, 0.0, "%.8g", input_flags);
        const bool height_changed = ImGui::InputDouble("Height", &draft_.height, 0.0, 0.0, "%.8g", input_flags);
        ImGui::PopItemWidth();
        ImGui::PopItemFlag();
        if (length_changed || width_changed || height_changed)
        {
            error_.clear();
        }

        ImGui::BeginDisabled(draft_ == preview.box_dimensions());
        if (ImGui::Button("Rebuild"))
        {
            if (auto result = preview.rebuild_box(draft_); !result)
            {
                error_ = std::move(result.error());
            }
            else
            {
                error_.clear();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            draft_ = preview.box_dimensions();
            error_.clear();
        }
        ImGui::EndDisabled();

        draw_history(preview);
        const auto [length, width, height] = preview.box_dimensions();
        ImGui::Text("Applied: %.8g x %.8g x %.8g", length, width, height);
        if (!error_.empty())
        {
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 20.0F * ImGui::GetFontSize());
            ImGui::Text("Rebuild failed: %s", error_.c_str());
            ImGui::PopTextWrapPos();
        }
    }
    ImGui::End();
}

void BoxEditor::draw_history(BRepPreview &preview)
{
    ImGui::BeginDisabled(!preview.can_undo_box());
    const bool undo = ImGui::Button("Undo");
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!preview.can_redo_box());
    const bool redo = ImGui::Button("Redo");
    ImGui::EndDisabled();
    if (!undo && !redo)
    {
        return;
    }

    const bool draft_is_clean = draft_ == preview.box_dimensions();
    if (undo)
    {
        preview.undo_box();
    }
    else
    {
        preview.redo_box();
    }

    if (draft_is_clean)
    {
        draft_ = preview.box_dimensions();
    }
    error_.clear();
}
