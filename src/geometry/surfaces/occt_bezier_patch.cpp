#include "occt_bezier_patch.hpp"

#include <NCollection_Array2.hxx>
#include <Standard_Failure.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <cmath>
#include <numbers>

namespace occt_lab::geometry
{
namespace
{
NCollection_Array2<gp_Pnt> occt_poles(const CubicBezierPatch::ControlNet &net)
{
    constexpr auto side = CubicBezierPatch::side_length;
    NCollection_Array2<gp_Pnt> poles(1, side, 1, side);
    for (std::size_t row = 0; row < side; ++row)
    {
        for (std::size_t column = 0; column < side; ++column)
        {
            const auto &[x, y, z] = net[row * side + column];
            // OCCT indexes (u, v); our array stores v rows with u increasing along each row.
            poles.SetValue(static_cast<int>(column) + 1, static_cast<int>(row) + 1, gp_Pnt{x, y, z});
        }
    }
    return poles;
}
} // namespace

OcctBezierPatch::OcctBezierPatch(const CubicBezierPatch &patch) : surface_(occt_poles(patch.control_net()))
{
}

auto OcctBezierPatch::evaluate(const double u, const double v) const -> std::expected<SurfaceSample, std::string>
{
    if (!std::isfinite(u) || !std::isfinite(v) || u < 0.0 || u > 1.0 || v < 0.0 || v > 1.0)
    {
        return std::unexpected("Choose finite u and v values between 0 and 1.");
    }
    try
    {
        const auto [point, du, dv] = surface_.EvalD1(u, v);
        const auto cross = du.Crossed(dv);
        const double length = std::hypot(cross.X(), cross.Y(), cross.Z());
        if (!std::isfinite(point.X()) || !std::isfinite(point.Y()) || !std::isfinite(point.Z()) ||
            !std::isfinite(length))
        {
            return std::unexpected("OCCT evaluation exceeded the finite coordinate range.");
        }
        if (length <= CubicBezierPatch::normal_tolerance)
        {
            return std::unexpected("The OCCT surface normal is undefined at this parameter.");
        }
        return SurfaceSample{.position = {.x = point.X(), .y = point.Y(), .z = point.Z()},
                             .normal = {.x = cross.X() / length, .y = cross.Y() / length, .z = cross.Z() / length}};
    }
    catch (const Standard_Failure &error)
    {
        return std::unexpected(error.what());
    }
}

SurfaceSampleDifference compare_surface_samples(const SurfaceSample &sample, const SurfaceSample &reference)
{
    const auto &[a, n] = sample;
    const auto &[b, m] = reference;
    const gp_Vec normal{n.x, n.y, n.z};
    const gp_Vec reference_normal{m.x, m.y, m.z};
    // atan2 retains small angular differences and distinguishes opposite normals.
    const double angle = std::atan2(normal.CrossMagnitude(reference_normal), normal.Dot(reference_normal));
    return {.position_distance_mm = std::hypot(a.x - b.x, a.y - b.y, a.z - b.z),
            .normal_angle_degrees = angle * (180.0 / std::numbers::pi)};
}
} // namespace occt_lab::geometry
