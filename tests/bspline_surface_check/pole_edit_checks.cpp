#include "application/editable_nurbs_surface.hpp"
#include "application/nurbs_surface_example.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>

void check_nurbs_pole_edits()
{
    EditableNurbsSurface model{occt_lab::make_nurbs_surface_example()};
    const auto original = model.current();
    auto poles = original.definition.poles;
    auto weights = original.definition.weights;
    poles[6].z += 6.0;
    weights[6] = 1.0;
    poles.back().x = 8.0;
    model.apply(poles, weights).value();

    const auto &edited = model.current();
    if (edited.definition.poles != poles || edited.definition.weights != weights || !model.can_undo())
    {
        throw std::runtime_error("Applying the NURBS control net lost an edit or its undo step");
    }
    // At u=3.5,v=0.5 the unit-weight point has z=14 and the edited pole's basis is 9/16.
    if (!(std::abs(edited.isolines.constant_v.at(5).at(16).z - 17.375) <= 1e-10))
    {
        throw std::runtime_error("The cached NURBS isoline did not reflect the new position and weight");
    }
    if (edited.isolines.constant_u.back().back() != poles.back())
    {
        throw std::runtime_error("The edited corner was not resampled");
    }
    const auto edited_lines = edited.isolines;
    model.undo();
    const auto &restored = model.current();
    if (restored.definition.poles != original.definition.poles ||
        restored.definition.weights != original.definition.weights ||
        restored.isolines.constant_u != original.isolines.constant_u ||
        restored.isolines.constant_v != original.isolines.constant_v)
    {
        throw std::runtime_error("A single undo should restore the complete NURBS surface and both isoline families");
    }

    model.apply(original.definition.poles, original.definition.weights).value();
    auto invalid_weights = weights;
    invalid_weights[6] = 0;
    if (model.apply(poles, invalid_weights))
    {
        throw std::runtime_error("A zero pole weight was accepted");
    }
    auto invalid_poles = poles;
    invalid_poles[6].z = std::numeric_limits<double>::quiet_NaN();
    if (model.apply(invalid_poles, weights))
    {
        throw std::runtime_error("A non-finite pole coordinate was accepted");
    }
    if (!model.can_redo() || restored.definition.poles != original.definition.poles ||
        restored.definition.weights != original.definition.weights ||
        restored.isolines.constant_v != original.isolines.constant_v)
    {
        throw std::runtime_error("A failed or unchanged edit altered the NURBS surface or its redo history");
    }
    model.redo();
    if (model.current().definition.poles != poles || model.current().definition.weights != weights ||
        model.current().isolines.constant_u != edited_lines.constant_u ||
        model.current().isolines.constant_v != edited_lines.constant_v)
    {
        throw std::runtime_error("Redo should restore the edited NURBS surface and its isolines");
    }
    model.undo();
    poles.back().y += 1;
    model.apply(poles, weights).value();
    if (model.can_redo() || !model.can_undo())
    {
        throw std::runtime_error("A new NURBS edit should replace the previous redo step");
    }
}
