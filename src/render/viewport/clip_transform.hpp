#pragma once

#include "geometry/vec3.hpp"

#include <array>

using ClipRow = std::array<float, 4>;

struct ClipTransform
{
    ClipRow x, y, z, w;

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
