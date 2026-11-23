#pragma once

#include <TopoDS_Solid.hxx>

#include <filesystem>

namespace occt_lab::document
{
struct PlateMeasurements
{
    double volume_mm3;
    double hole_wall_area_mm2;
};

void write_plate_step(const std::filesystem::path &path, const TopoDS_Solid &plate);
[[nodiscard]] TopoDS_Solid read_step_solid(const std::filesystem::path &path);
[[nodiscard]] PlateMeasurements verify_plate_step(const TopoDS_Solid &original, const TopoDS_Solid &imported);
} // namespace occt_lab::document
