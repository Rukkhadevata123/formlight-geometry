#pragma once

namespace occt_lab::geometry
{
struct Vec3
{
    double x;
    double y;
    double z;

    bool operator==(const Vec3 &) const = default;
};
} // namespace occt_lab::geometry
