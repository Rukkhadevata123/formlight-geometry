export module formlight_geometry.application.bezier_patch_presets;

import std;
import formlight_geometry.geometry.surfaces.bezier_patch;

export namespace formlight_geometry
{
[[nodiscard]] constexpr geometry::CubicBezierPatch::ControlNet planar_bezier_net()
{
    constexpr auto side = geometry::CubicBezierPatch::side_length;
    geometry::CubicBezierPatch::ControlNet net{};
    for (std::size_t row = 0; row < side; ++row)
    {
        for (std::size_t column = 0; column < side; ++column)
        {
            net[row * side + column] = {.x = -45.0 + 30.0 * static_cast<double>(column),
                                        .y = -36.0 + 24.0 * static_cast<double>(row),
                                        .z = 0.0};
        }
    }
    return net;
}

[[nodiscard]] constexpr geometry::CubicBezierPatch::ControlNet raised_bezier_net()
{
    auto net = planar_bezier_net();
    constexpr auto side = geometry::CubicBezierPatch::side_length;
    net[side + 1].z = 40;
    net[side + 2].z = 28;
    net[2 * side + 1].z = 14;
    net[2 * side + 2].z = 32;
    return net;
}
} // namespace formlight_geometry
