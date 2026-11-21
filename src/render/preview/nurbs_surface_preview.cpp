#include "nurbs_surface_preview.hpp"

#include "application/nurbs_surface_example.hpp"
#include "surface_wireframe.hpp"

#include <Bnd_Box.hxx>

#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <utility>

NurbsSurfacePreview::NurbsSurfacePreview() : definition_(occt_lab::make_nurbs_surface_example())
{
    const occt_lab::geometry::BSplineSurface surface{definition_};
    auto lines = surface.isolines();
    if (!lines)
    {
        throw std::runtime_error(lines.error());
    }
    isolines_ = std::move(*lines);

    Bnd_Box bounds;
    for (const auto &[x, y, z] : definition_.poles)
    {
        bounds.Add(gp_Pnt{x, y, z});
    }
    const auto low = bounds.CornerMin();
    const auto high = bounds.CornerMax();
    center_ = {std::midpoint(low.X(), high.X()), std::midpoint(low.Y(), high.Y()), std::midpoint(low.Z(), high.Z())};
    const double span = std::max({high.X() - low.X(), high.Y() - low.Y(), high.Z() - low.Z()});
    // Frame the longest side in the same 90-unit view as the Bezier example.
    scale_ = span > 0.0 ? span / 90.0 : 1.0;
}

occt_lab::render::ViewportProjection NurbsSurfacePreview::projection(const ClipTransform &transform) const
{
    return {transform, *ImGui::GetMainViewport(), center_, scale_};
}

std::optional<std::size_t> NurbsSurfacePreview::pick_pole(const ClipTransform &transform, const ImVec2 cursor) const
{
    return projection(transform).pick_pole(definition_.poles, cursor);
}

void NurbsSurfacePreview::draw(const ClipTransform &transform, const PatchLayers &layers,
                               const std::size_t selected_pole) const
{
    draw_surface_wireframe(*ImGui::GetBackgroundDrawList(ImGui::GetMainViewport()), projection(transform),
                           definition_.poles, definition_.u.pole_count, isolines_, layers, selected_pole);
}
