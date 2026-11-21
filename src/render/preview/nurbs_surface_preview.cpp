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

void NurbsSurfacePreview::draw(const ClipTransform &transform, const PatchLayers &layers) const
{
    auto *viewport = ImGui::GetMainViewport();
    const occt_lab::render::ViewportProjection project{transform, *viewport, center_, scale_};
    draw_surface_wireframe(*ImGui::GetBackgroundDrawList(viewport), project, definition_.poles,
                           definition_.u.pole_count, isolines_, layers);
}
