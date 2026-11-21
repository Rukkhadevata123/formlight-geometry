#pragma once

#include "geometry/surfaces/bezier_patch.hpp"

#include <iosfwd>

namespace occt_lab::document::io
{
struct BezierPatchData
{
    geometry::CubicBezierPatch::ControlNet poles;
    geometry::PatchSampling sampling;
};

// The caller has consumed the file type; the version is the next token.
[[nodiscard]] BezierPatchData read_bezier_patch(std::istream &in);
[[nodiscard]] geometry::PatchSampling read_sampling(std::istream &in);
void write_sampling(std::ostream &out, geometry::PatchSampling sampling);
} // namespace occt_lab::document::io
