#include "orbit_camera.hpp"

#include <gp_Dir.hxx>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace
{
ClipRow clip_row(const gp_Dir &direction, const double scale, const double offset = 0.0)
{
    return {static_cast<float>(scale * direction.X()), static_cast<float>(scale * direction.Y()),
            static_cast<float>(scale * direction.Z()), static_cast<float>(offset)};
}
} // namespace

void OrbitCamera::orbit(const float delta_x, const float delta_y)
{
    constexpr double degrees_per_pixel = 0.35;
    constexpr double max_elevation = 89.0;
    // Move the camera opposite the horizontal drag so the near side follows the cursor.
    azimuth_degrees_ = std::remainder(azimuth_degrees_ - delta_x * degrees_per_pixel, 360.0);
    elevation_degrees_ = std::clamp(elevation_degrees_ + delta_y * degrees_per_pixel, -max_elevation, max_elevation);
}

void OrbitCamera::zoom(const float wheel_delta)
{
    constexpr double distance_per_notch = 1.15;
    constexpr double min_distance = 100.0;
    constexpr double max_distance = 800.0;
    distance_ = std::clamp(distance_ * std::pow(distance_per_notch, -wheel_delta), min_distance, max_distance);
}

ClipTransform OrbitCamera::clip_transform(const std::uint32_t width, const std::uint32_t height,
                                          const ProjectionMode mode, const double view_scale) const
{
    constexpr double radians_per_degree = std::numbers::pi / 180.0;
    const double azimuth = azimuth_degrees_ * radians_per_degree;
    const double elevation = elevation_degrees_ * radians_per_degree;
    const double horizontal = std::cos(elevation);
    const gp_Dir towards_camera{horizontal * std::cos(azimuth), horizontal * std::sin(azimuth), std::sin(elevation)};
    const gp_Dir right{-std::sin(azimuth), std::cos(azimuth), 0.0};
    const gp_Dir up = towards_camera.Crossed(right);

    // Normalize the vertex scale before projection so zoom and clipping are relative to the model size.
    const double inverse_scale = 1.0 / view_scale;
    constexpr double view_half_extent = 85.0;
    const double focal_scale = inverse_scale * initial_distance * std::min(width, height) / view_half_extent;
    const double focal_x = focal_scale / width;
    const double focal_y = focal_scale / height;
    if (mode == ProjectionMode::orthographic)
    {
        constexpr double depth_extent = 200.0;
        return {
            .x = clip_row(right, focal_x / distance_),
            .y = clip_row(up, -focal_y / distance_),
            .z = clip_row(towards_camera, -inverse_scale / depth_extent, 0.5),
            .w = {0.0F, 0.0F, 0.0F, 1.0F},
        };
    }

    // Perspective zoom moves the eye along the orbit radius. Vulkan depth after division is in [0, 1].
    constexpr double near_plane = 10.0;
    constexpr double far_plane = 1000.0;
    constexpr double depth_scale = far_plane / (far_plane - near_plane);
    return {
        .x = clip_row(right, focal_x),
        .y = clip_row(up, -focal_y),
        .z = clip_row(towards_camera, -depth_scale * inverse_scale, depth_scale * (distance_ - near_plane)),
        .w = clip_row(towards_camera, -inverse_scale, distance_),
    };
}
