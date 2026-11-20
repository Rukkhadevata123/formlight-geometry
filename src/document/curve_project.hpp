#pragma once

#include "geometry/bspline_curve.hpp"

#include <expected>
#include <filesystem>
#include <string>

namespace occt_lab::document
{
struct CurveProjectSettings
{
    int object_id = 1;
    std::string name = "B-spline";
    int pole_number = 1;
    double knot_parameter = 0.5;
    double fit_tolerance = 1.0;
    bool operator==(const CurveProjectSettings &) const = default;
};

struct CurveProject
{
    geometry::SampledCurve curve;
    CurveProjectSettings settings;
};

[[nodiscard]] auto save_curve_project(const std::filesystem::path &path, const geometry::SampledCurve &curve,
                                      const CurveProjectSettings &settings) -> std::expected<void, std::string>;
[[nodiscard]] auto open_curve_project(const std::filesystem::path &path) -> std::expected<CurveProject, std::string>;
} // namespace occt_lab::document
