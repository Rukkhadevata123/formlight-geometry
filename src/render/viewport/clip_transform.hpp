#pragma once

#include "geometry/vec3.hpp"

#include <array>
#include <limits>
#include <optional>

using ClipRow = std::array<float, 4>;

struct ClipTransform
{
    ClipRow x, y, z, w;

    // At fixed model Z, clip.x = ndc_x * clip.w and clip.y = ndc_y * clip.w are linear in X and Y.
    [[nodiscard]] constexpr auto intersect_z_plane(double ndc_x, double ndc_y, double plane_z) const
        -> std::optional<occt_lab::geometry::Vec3>
    {
        using occt_lab::geometry::Vec3;
        const auto coefficients = [&](const ClipRow &row, double ndc) -> Vec3
        { return {row[0] - ndc * w[0], row[1] - ndc * w[1], row[2] - ndc * w[2]}; };
        const Vec3 a = coefficients(x, ndc_x);
        const Vec3 b = coefficients(y, ndc_y);
        const Vec3 ray = cross(a, b);
        constexpr double parallel_tolerance = std::numeric_limits<float>::epsilon();
        const double length_squared = ray.x * ray.x + ray.y * ray.y + ray.z * ray.z;
        if (ray.z * ray.z <= parallel_tolerance * parallel_tolerance * length_squared)
        {
            return std::nullopt;
        }
        const double rhs_x = ndc_x * w[3] - x[3] - a.z * plane_z;
        const double rhs_y = ndc_y * w[3] - y[3] - b.z * plane_z;
        const Vec3 hit{(rhs_x * b.y - a.y * rhs_y) / ray.z, (a.x * rhs_y - rhs_x * b.x) / ray.z, plane_z};
        const auto component = [&](const ClipRow &row)
        { return row[0] * hit.x + row[1] * hit.y + row[2] * hit.z + row[3]; };
        const double depth = component(z);
        const double clip_w = component(w);
        if (clip_w > 0 && clip_w < std::numeric_limits<double>::infinity() && depth >= 0 && depth <= clip_w)
        {
            return hit;
        }
        return std::nullopt;
    }

    // GPU vertices are relative to origin, while the wireframe uses (world - center) / scale.
    [[nodiscard]] constexpr ClipTransform from_local_frame(occt_lab::geometry::Vec3 origin,
                                                           occt_lab::geometry::Vec3 center, double scale) const
    {
        const occt_lab::geometry::Vec3 offset{(origin.x - center.x) / scale, (origin.y - center.y) / scale,
                                              (origin.z - center.z) / scale};
        const auto local_row = [&](const ClipRow &row) -> ClipRow
        {
            return {static_cast<float>(row[0] / scale), static_cast<float>(row[1] / scale),
                    static_cast<float>(row[2] / scale),
                    static_cast<float>(row[3] + row[0] * offset.x + row[1] * offset.y + row[2] * offset.z)};
        };
        return {local_row(x), local_row(y), local_row(z), local_row(w)};
    }
};
