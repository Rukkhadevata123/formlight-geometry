#include "application/editable_nurbs_surface.hpp"
#include "application/nurbs_surface_example.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

void check_nurbs_presets()
{
    const auto saddle = occt_lab::make_saddle_surface();
    const occt_lab::geometry::BSplineSurface surface{saddle};
    for (const double u : {0.0, 0.25, 0.5, 0.75, 1.0})
    {
        for (const double v : {0.0, 0.25, 0.5, 0.75, 1.0})
        {
            const double s = 2 * u - 1;
            const double t = 2 * v - 1;
            // The center pole is at the origin. Its extra unit of weight changes only the denominator.
            const double w = 1 + 4 * u * (1 - u) * v * (1 - v);
            const auto p = surface.position(u, v).value();
            if (!(std::hypot(p.x - 45 * s / w, p.y - 36 * t / w, p.z - 12 * (s * s - t * t) / w) <= 1e-10))
            {
                throw std::runtime_error("Saddle preset differs from its rational quadratic surface");
            }
        }
    }

    EditableNurbsSurface model{saddle};
    const auto saddle_frame = model.current().framing;
    auto poles = saddle.poles;
    poles.back().x += 10;
    model.apply(poles, saddle.weights).value();
    if (model.current().framing != saddle_frame)
    {
        throw std::runtime_error("Editing an outer saddle pole changed the framing");
    }
    const auto edited = model.current();
    const auto analytic = occt_lab::make_nurbs_surface_example();
    model.replace(analytic).value();
    const auto analytic_frame = model.current().framing;
    if (model.current().definition != analytic || analytic_frame == saddle_frame)
    {
        throw std::runtime_error("Creating the analytic preset did not replace the geometry and framing");
    }
    model.replace(analytic).value();
    model.undo();
    if (model.current().definition != edited.definition || model.current().framing != saddle_frame ||
        model.current().isolines.constant_v != edited.isolines.constant_v)
    {
        throw std::runtime_error("Undo should restore the edited saddle, its isolines and its original framing");
    }
    auto invalid = analytic;
    invalid.poles.pop_back();
    if (model.replace(std::move(invalid)) || !model.can_redo() || model.current().definition != edited.definition)
    {
        throw std::runtime_error("An invalid replacement disturbed the edited saddle or its redo step");
    }
    model.redo();
    if (model.current().definition != analytic || model.current().framing != analytic_frame)
    {
        throw std::runtime_error("Redo should restore the analytic preset and its framing");
    }
}
