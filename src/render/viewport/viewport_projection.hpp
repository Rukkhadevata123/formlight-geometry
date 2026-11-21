#pragma once

#include "geometry/vec3.hpp"

#include <imgui.h>

#include <optional>
#include <span>

struct ClipTransform;

namespace occt_lab::render
{
struct ClipPoint;

class ViewportProjection
{
  public:
    ViewportProjection(const ClipTransform &transform, const ImGuiViewport &viewport, double framing_extent);
    [[nodiscard]] std::optional<ImVec2> visible_point(const geometry::Vec3 &world) const;
    [[nodiscard]] std::optional<std::size_t> pick_pole(std::span<const geometry::Vec3> poles, ImVec2 cursor) const;
    [[nodiscard]] geometry::Vec3 dragged_position(const geometry::Vec3 &origin, ImVec2 screen_delta) const;
    void draw_polyline(ImDrawList &draw_list, std::span<const geometry::Vec3> points, ImU32 color, float width) const;

  private:
    [[nodiscard]] ClipPoint clip_point(const geometry::Vec3 &world) const;
    [[nodiscard]] ImVec2 screen(const ClipPoint &point) const;

    const ClipTransform &transform_;
    const ImGuiViewport &viewport_;
    double scale_;
};
} // namespace occt_lab::render
