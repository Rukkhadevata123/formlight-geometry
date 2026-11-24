import std;
import occt_lab.application.editable_nurbs_surface;
import occt_lab.application.nurbs_surface_creation;
import occt_lab.document.nurbs_surface_project;
import occt_lab.geometry.surfaces.bspline_surface;
import occt_lab.test.bspline_surface_check.analytic_surface;
import occt_lab.test.support.project_test_directory;

namespace
{
using namespace occt_lab;
using geometry::SurfaceKnotDirection;

void check_weighted_insertion(const SurfaceKnotDirection direction)
{
    auto original = test::make_analytic_surface();
    for (auto &weight : original.weights)
    {
        weight *= 0.25;
    }
    EditableNurbsSurface model(original);
    const auto before = model.current();
    const bool along_u = direction == SurfaceKnotDirection::u;
    const double parameter = along_u ? 4 : -0.75;
    model.insert_knot(direction, parameter).value();
    const auto after = model.current();
    const auto &refined = after.definition;
    if (refined.u.pole_count != (along_u ? 5 : 4) || refined.v.pole_count != (along_u ? 3 : 4) ||
        (along_u ? refined.v != original.v : refined.u != original.u) || after.framing != before.framing)
    {
        throw std::runtime_error("Knot insertion changed the wrong axis or the viewport framing");
    }
    const auto &axis = along_u ? refined.u : refined.v;
    if (!std::ranges::contains(axis.knots, geometry::BSplineKnot{parameter, 1}) ||
        refined.weights.front() != original.weights.front())
    {
        throw std::runtime_error("Insertion lost the new knot or the original weight scale");
    }
    test::check_refined_positions(*before.surface, *after.surface);
    model.undo();
    for (const double invalid : {2.0, -5.0, std::numeric_limits<double>::quiet_NaN()})
    {
        if (model.insert_knot(direction, invalid) || model.current().definition != original || !model.can_redo())
        {
            throw std::runtime_error("Rejected knot insertion changed the surface or redo history");
        }
    }
    model.redo();
    if (model.current().definition != refined)
    {
        throw std::runtime_error("Redo did not restore the refined control net");
    }
    if (along_u)
    {
        model.insert_knot(direction, parameter).value();
        if (!std::ranges::contains(model.current().definition.u.knots, geometry::BSplineKnot{parameter, 2}))
        {
            throw std::runtime_error("Inserting an existing U knot did not increment its multiplicity");
        }
    }
    if (model.insert_knot(direction, parameter))
    {
        throw std::runtime_error("Accepted an interior knot above the axis degree");
    }
}

void check_grid_project(const bool approximate)
{
    SurfacePoleRows rows;
    resize_surface_net(rows, 4, 3);
    rows[1][2].position.z = 0.5;
    EditableNurbsSurface model(
        (approximate ? approximate_surface_grid(rows, 1.0) : interpolate_surface_grid(rows)).value());
    const ProjectTestDirectory directory;
    const auto path = directory.path / "refine.occtlab";
    const auto check_version = [&](const int expected)
    {
        std::ifstream in(path);
        std::string header;
        std::getline(in, header);
        if (header != std::format("occt-lab-nurbs-surface {}", expected))
        {
            throw std::runtime_error(std::format("Expected surface project v{}, got '{}'", expected, header));
        }
    };
    document::save_nurbs_surface_project(path, model).value();
    check_version(approximate ? 4 : 3);
    auto loaded = document::open_nurbs_surface_project(path).value();
    const auto original = loaded.current();
    loaded.insert_knot(SurfaceKnotDirection::u, 0.4).value();
    loaded.insert_knot(SurfaceKnotDirection::v, 0.6).value();
    const auto &refined = loaded.current();
    if (refined.definition.data_grid != original.definition.data_grid ||
        std::abs(refined.max_grid_deviation->distance_mm - original.max_grid_deviation->distance_mm) > 1e-11)
    {
        throw std::runtime_error("Refining the surface changed its source grid or deviation");
    }
    document::save_nurbs_surface_project(path, loaded).value();
    check_version(approximate ? 4 : 5);
    auto reopened = document::open_nurbs_surface_project(path).value();
    if (reopened.current().definition != refined.definition ||
        reopened.current().max_grid_deviation != refined.max_grid_deviation)
    {
        throw std::runtime_error("Reopening lost the independent grid size or the deviation marker");
    }
}
} // namespace

void check_surface_knot_insertion()
{
    check_weighted_insertion(SurfaceKnotDirection::u);
    check_weighted_insertion(SurfaceKnotDirection::v);
    check_grid_project(false);
    check_grid_project(true);
}
