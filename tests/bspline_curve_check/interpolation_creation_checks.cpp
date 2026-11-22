#include "../support/project_test_directory.hpp"
#include "application/editable_curve.hpp"
#include "document/curve_project.hpp"

#include <array>
#include <stdexcept>

void check_interpolated_curve_creation()
{
    using occt_lab::geometry::Vec3;
    // Chord lengths of 10, 20 and 10 give known parameters in all three dimensions.
    constexpr std::array<Vec3, 4> points{{{0, 0, 0}, {10, 0, 0}, {10, 0, 20}, {10, -10, 20}}};
    constexpr std::array parameters{0.0, 0.25, 0.75, 1.0};
    EditableCurve curve;
    const auto previous = curve.current();
    if (!curve.create_interpolated_curve(points))
    {
        throw std::runtime_error("Could not interpolate the supplied XYZ rows");
    }
    const auto interpolant = curve.current();
    if (interpolant.data_points.size() != points.size())
    {
        throw std::runtime_error("Creation lost interpolation data points");
    }
    for (std::size_t i = 0; i < points.size(); ++i)
    {
        const auto &[position, parameter, residual] = interpolant.data_points[i];
        if (position != points[i] || parameter != parameters[i] || !(residual <= 1e-10))
        {
            throw std::runtime_error("The interpolant must pass through each XYZ row at its chord parameter");
        }
    }
    curve.undo();
    if (curve.current() != previous)
    {
        throw std::runtime_error("Undo did not restore the curve preceding XYZ input");
    }
    auto invalid = points;
    invalid[1] = invalid[0];
    if (curve.create_interpolated_curve(invalid) || !curve.can_redo() || curve.current() != previous)
    {
        throw std::runtime_error("Rejecting adjacent duplicate rows must preserve the previous curve and redo");
    }
    curve.redo();
    if (curve.current() != interpolant)
    {
        throw std::runtime_error("Redo lost the interpolant or its data points");
    }

    const ProjectTestDirectory directory;
    const auto path = directory.path / "through-points.occtlab";
    if (!occt_lab::document::save_curve_project(path, curve.current(), {}))
    {
        throw std::runtime_error("Could not save the XYZ interpolant");
    }
    const auto opened = occt_lab::document::open_curve_project(path);
    if (!opened || opened->curve != interpolant)
    {
        throw std::runtime_error("The XYZ interpolant or its data points changed on reopening");
    }

    const auto pair = std::span{points}.first<2>();
    if (!curve.create_interpolated_curve(pair) || curve.current().data_points.size() != 2 ||
        curve.current().samples.front() != pair.front() || curve.current().samples.back() != pair.back())
    {
        throw std::runtime_error("The minimal two-row input must interpolate both endpoints");
    }
}
