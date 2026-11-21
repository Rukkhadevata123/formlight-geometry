#include "box_model.hpp"

#include <BRepCheck_Analyzer.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <Precision.hxx>

#include <cmath>
#include <format>
#include <stdexcept>
#include <string_view>

namespace
{
void validate_dimension(const double dimension, const std::string_view name)
{
    if (constexpr double tolerance = Precision::Confusion(); !std::isfinite(dimension) || dimension <= tolerance)
    {
        throw std::invalid_argument(std::format("{} must be finite and greater than {:g} mm", name, tolerance));
    }
}
} // namespace

BoxModel make_box(const BoxDimensions &dimensions)
{
    const auto [length, width, height] = dimensions;
    validate_dimension(length, "Length");
    validate_dimension(width, "Width");
    validate_dimension(height, "Height");

    BRepPrimAPI_MakeBox builder{length, width, height};
    builder.Build();
    if (!builder.IsDone())
    {
        throw std::runtime_error("OCCT box construction failed");
    }

    const auto &shape = builder.Shape();
    if (shape.IsNull() || !BRepCheck_Analyzer{shape}.IsValid())
    {
        throw std::runtime_error("OCCT box failed B-Rep validation");
    }
    return {.dimensions = dimensions, .shape = shape};
}
