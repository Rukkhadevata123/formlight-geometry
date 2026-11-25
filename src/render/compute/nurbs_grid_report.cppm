module;

#include <gp_Vec.hxx>

export module formlight_geometry.render.compute.nurbs_grid_report;

import std;
import formlight_geometry.geometry.surfaces.bspline_surface;
import formlight_geometry.render.compute.nurbs_grid_input;

namespace formlight_geometry::render
{
export [[nodiscard]] std::string compare_nurbs_grid(const std::span<const NurbsGridVertex> vertices,
                                                    const std::span<const geometry::SurfaceProbe> reference,
                                                    const geometry::Vec3 origin, const double position_tolerance_mm)
{
    if (vertices.empty() || vertices.size() != reference.size())
    {
        return "Failed: GPU and OCCT grids must contain the same nonzero number of vertices.";
    }
    constexpr double angle_tolerance_deg = 1e-3;
    double max_distance = 0, squared_distance = 0, max_angle = 0;
    std::size_t compared = 0, unavailable = 0, mismatches = 0;
    for (std::size_t i = 0; i < vertices.size(); ++i)
    {
        const auto &[p, n] = vertices[i];
        const auto &[point, normal] = reference[i];
        const double distance =
            std::hypot((point.x - origin.x) - p[0], (point.y - origin.y) - p[1], (point.z - origin.z) - p[2]);
        if (!std::isfinite(distance) || !std::isfinite(p[3]) || p[3] <= 0)
        {
            return std::format("Failed: invalid GPU position or weight at vertex {}.", i);
        }
        max_distance = std::max(max_distance, distance);
        squared_distance += distance * distance;

        const gp_Vec gpu{n[0], n[1], n[2]};
        const double length = gpu.Magnitude();
        const bool has_normal = n[3] == 1;
        if (!((has_normal && std::abs(length - 1.0) <= 1e-4) || (n[3] == 0 && length == 0)))
        {
            return std::format("Failed: invalid GPU normal at vertex {}.", i);
        }
        if (has_normal != normal.has_value())
        {
            ++mismatches;
        }
        else if (!has_normal)
        {
            ++unavailable;
        }
        else
        {
            ++compared;
            const gp_Vec cpu{normal->x, normal->y, normal->z};
            const double radians = std::atan2(gpu.CrossMagnitude(cpu), gpu.Dot(cpu));
            max_angle = std::max(max_angle, radians * 180.0 / std::numbers::pi);
        }
    }
    const bool passed = max_distance <= position_tolerance_mm && max_angle <= angle_tolerance_deg && mismatches == 0;
    const auto angle = compared ? std::format("{:.4g} deg", max_angle) : std::string{"n/a"};
    return std::format("{}: {} positions\nMax / RMS: {:.4g} / {:.4g} mm (limit {:g} mm)\n"
                       "Normals: {} compared, {} unavailable on both\nAvailability mismatches: {}\n"
                       "Max oriented angle: {} (limit {:g} deg)",
                       passed ? "Passed" : "Failed", vertices.size(), max_distance,
                       std::sqrt(squared_distance / static_cast<double>(vertices.size())), position_tolerance_mm,
                       compared, unavailable, mismatches, angle, angle_tolerance_deg);
}
} // namespace formlight_geometry::render
