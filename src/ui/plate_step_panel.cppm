module;

#include <TopoDS_Solid.hxx>
#include <imgui.h>
#include <imgui_stdlib.h>

export module occt_lab.ui.plate_step_panel;

import std;
import occt_lab.document.plate_step;

export struct PlateStepPanel
{
    std::string path = "plate.step";
    std::string status;

    void draw(const TopoDS_Solid &plate);
};

void PlateStepPanel::draw(const TopoDS_Solid &plate)
{
    const auto *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({viewport->WorkPos.x + viewport->WorkSize.x - 16.0F, viewport->WorkPos.y + 16.0F},
                            ImGuiCond_FirstUseEver, {1.0F, 0.0F});
    if (ImGui::Begin("Plate STEP", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (ImGui::InputText("File path", &path))
        {
            status.clear();
        }
        ImGui::BeginDisabled(path.empty());
        const bool exporting = ImGui::Button("Export STEP");
        ImGui::SameLine();
        const bool reading = ImGui::Button("Read and compare");
        ImGui::EndDisabled();

        if (exporting || reading)
        {
            try
            {
                const auto file =
                    std::filesystem::absolute(std::filesystem::path{std::u8string(path.begin(), path.end())});
                if (exporting)
                {
                    occt_lab::document::write_plate_step(file, plate);
                    const auto utf8 = file.u8string();
                    status = "Exported " + std::string(utf8.begin(), utf8.end());
                }
                else
                {
                    const auto imported = occt_lab::document::read_step_solid(file);
                    const auto measures = occt_lab::document::verify_plate_step(plate, imported);
                    status = std::format("Verified one solid\nVolume: {:.6f} mm³\nHole wall: {:.6f} mm²",
                                         measures.volume_mm3, measures.hole_wall_area_mm2);
                }
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
    ImGui::End();
}
