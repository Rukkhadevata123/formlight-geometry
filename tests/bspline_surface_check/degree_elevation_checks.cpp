import std;
import formlight_geometry.application.editable_nurbs_surface;
import formlight_geometry.application.nurbs_surface_creation;
import formlight_geometry.document.nurbs_surface_project;
import formlight_geometry.geometry.surfaces.bspline_surface;
import formlight_geometry.test.bspline_surface_check.analytic_surface;
import formlight_geometry.test.support.project_test_directory;

namespace
{
using namespace formlight_geometry;
using geometry::SurfaceDirection;

void check_rational_elevation(const SurfaceDirection direction)
{
    auto definition = test::make_analytic_surface();
    for (auto &weight : definition.weights)
    {
        weight *= 0.25;
    }
    EditableNurbsSurface model(definition);
    const auto before = model.current();
    model.elevate_degree(direction).value();
    const auto after = model.current();
    const auto &elevated = after.definition;
    const bool along_u = direction == SurfaceDirection::u;
    const auto &axis = along_u ? definition.u : definition.v;
    const auto &changed = along_u ? elevated.u : elevated.v;
    if (changed.degree != axis.degree + 1 || changed.knots.size() != axis.knots.size() ||
        changed.pole_count != axis.pole_count + static_cast<int>(axis.knots.size()) - 1 ||
        (along_u ? elevated.v != definition.v : elevated.u != definition.u))
    {
        throw std::runtime_error("Degree elevation changed the wrong axis or its spline dimensions");
    }
    for (std::size_t i = 0; i < axis.knots.size(); ++i)
    {
        if (axis.knots[i].value != changed.knots[i].value ||
            axis.knots[i].multiplicity + 1 != changed.knots[i].multiplicity)
        {
            throw std::runtime_error("Degree elevation changed a knot value or the continuity at that knot");
        }
    }
    if (after.framing != before.framing || elevated.weights.front() != definition.weights.front())
    {
        throw std::runtime_error("Degree elevation lost the viewport framing or original weight scale");
    }
    test::check_refined_positions(*before.surface, *after.surface);
    model.undo();
    if (model.current().definition != definition || !model.can_redo())
    {
        throw std::runtime_error("Undo lost the original surface");
    }
    model.redo();
    if (model.current().definition != elevated)
    {
        throw std::runtime_error("Redo lost the elevated surface");
    }
}

void check_grid_elevation(const bool approximate)
{
    SurfacePoleRows rows;
    resize_surface_net(rows, 4, 3);
    rows[1][2].position.z = 0.5;
    EditableNurbsSurface model(
        (approximate ? approximate_surface_grid(rows, 1.0) : interpolate_surface_grid(rows)).value());
    const auto before = model.current();
    model.elevate_degree(SurfaceDirection::u).value();
    model.elevate_degree(SurfaceDirection::v).value();
    const auto &after = model.current();
    if (after.definition.data_grid != before.definition.data_grid ||
        std::abs(after.max_grid_deviation->distance_mm - before.max_grid_deviation->distance_mm) > 1e-11)
    {
        throw std::runtime_error("Degree elevation changed the source samples or their deviation");
    }
    const ProjectTestDirectory dir;
    const auto path = dir.path / "elevated.occtlab";
    document::save_nurbs_surface_project(path, model).value();
    const auto loaded = document::open_nurbs_surface_project(path).value();
    if (loaded.current().definition != after.definition ||
        loaded.current().max_grid_deviation != after.max_grid_deviation)
    {
        throw std::runtime_error("Reopening lost the elevated surface or its deviation marker");
    }
}
} // namespace

void check_surface_degree_elevation()
{
    check_rational_elevation(SurfaceDirection::u);
    check_rational_elevation(SurfaceDirection::v);
    check_grid_elevation(false);
    check_grid_elevation(true);

    const int limit = geometry::max_surface_degree();
    EditableNurbsSurface model(
        make_planar_nurbs_surface({.u_poles = limit + 1, .v_poles = 2, .u_degree = limit}).value());
    const auto initial = model.current().definition;
    model.elevate_degree(SurfaceDirection::v).value();
    model.undo();
    if (model.elevate_degree(SurfaceDirection::u) || model.current().definition != initial || !model.can_redo())
    {
        throw std::runtime_error("Attempting to exceed the U degree limit corrupted the model or redo history");
    }
}
