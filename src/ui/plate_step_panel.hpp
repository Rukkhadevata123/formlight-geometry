#pragma once

#include <string>

class TopoDS_Solid;

struct PlateStepPanel
{
    std::string path = "plate.step";
    std::string status;

    void draw(const TopoDS_Solid &plate);
};
