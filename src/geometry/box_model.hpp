#pragma once

#include <TopoDS_Shape.hxx>

struct BoxDimensions
{
    double length = 100.0;
    double width = 80.0;
    double height = 20.0;

    bool operator==(const BoxDimensions &) const = default;
};

struct BoxModel
{
    BoxDimensions dimensions;
    TopoDS_Shape shape;
};

[[nodiscard]] BoxModel make_box(const BoxDimensions &dimensions);
