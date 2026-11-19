#pragma once

#include "geometry/vec3.hpp"

#include <array>

struct ClipTransform;

class CurvePreview
{
  public:
    CurvePreview();

    void draw(const ClipTransform &transform) const;

  private:
    static constexpr std::size_t sample_count = 129;
    std::array<occt_lab::geometry::Vec3, sample_count> samples_{};
};
