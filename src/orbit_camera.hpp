#pragma once

#include <array>
#include <cstdint>

enum class ProjectionMode
{
    perspective,
    orthographic,
};

using ClipRow = std::array<float, 4>;

struct ClipTransform
{
    ClipRow x;
    ClipRow y;
    ClipRow z;
    ClipRow w;
};

class OrbitCamera
{
  public:
    void orbit(float delta_x, float delta_y);
    void zoom(float wheel_delta);

    [[nodiscard]] ClipTransform clip_transform(std::uint32_t width, std::uint32_t height, ProjectionMode mode) const;

  private:
    static constexpr double initial_distance = 230.0;

    double azimuth_degrees_ = 45.0;
    double elevation_degrees_ = 32.5;
    double distance_ = initial_distance;
};
