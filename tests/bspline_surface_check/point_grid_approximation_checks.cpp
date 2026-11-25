import std;
import formlight_geometry.application.editable_nurbs_surface;
import formlight_geometry.application.nurbs_surface_creation;
import formlight_geometry.document.nurbs_surface_project;
import formlight_geometry.geometry.surfaces.bspline_surface;
import formlight_geometry.test.support.project_test_directory;

namespace
{
using namespace formlight_geometry;

double measured_error(const geometry::BSplineSurfaceDefinition &definition, const SurfacePoleRows &rows)
{
    const geometry::BSplineSurface surface(definition);
    double largest = 0;
    for (std::size_t v = 0; v < rows.size(); ++v)
    {
        for (std::size_t u = 0; u < rows[v].size(); ++u)
        {
            const double pu = static_cast<double>(u) / static_cast<double>(rows[v].size() - 1);
            const double pv = static_cast<double>(v) / static_cast<double>(rows.size() - 1);
            const auto p = surface.position(pu, pv).value();
            const auto &q = rows[v][u].position;
            largest = std::max(largest, std::hypot(p.x - q.x, p.y - q.y, p.z - q.z));
        }
    }
    return largest;
}

void check_cubic_fit(const int columns, const int rows)
{
    SurfacePoleRows grid;
    resize_surface_net(grid, columns, rows);
    for (int v = 0; v < rows; ++v)
    {
        for (int u = 0; u < columns; ++u)
        {
            grid[v][u].position.z = 0.003 * u * u * u - 0.01 * v * v * v + 0.04 * u * v;
        }
    }
    const auto fit = approximate_surface_grid(grid, 1e-8).value();
    if (fit.u.pole_count != std::min(4, columns) || fit.v.pole_count != std::min(4, rows) ||
        measured_error(fit, grid) > 1e-8)
    {
        throw std::runtime_error("The cubic fit lost its parameter axes or required excess control points");
    }
}
} // namespace

void check_point_grid_approximation()
{
    using namespace formlight_geometry;
    check_cubic_fit(64, 3);
    check_cubic_fit(2, 8);
    SurfacePoleRows grid;
    resize_surface_net(grid, 9, 7);
    for (auto &row : grid)
    {
        for (std::size_t u = 0; u < row.size(); ++u)
        {
            // A cubic with one interior knot at u = 0.5.
            const double right = std::max(0.0, static_cast<double>(u) - 4);
            row[u].position.z = right * right * right;
        }
    }
    const auto multispan = approximate_surface_grid(grid, 1e-8).value();
    if (multispan.u.knots.size() != 3 || measured_error(multispan, grid) > 1e-8)
    {
        throw std::runtime_error("A two-span cubic grid did not recover its interior knot");
    }
    grid.clear();
    resize_surface_net(grid, 4, 3);
    grid[1][2].position.z = 0.5;
    const auto loose = approximate_surface_grid(grid, 1.0).value();
    const double deviation = measured_error(loose, grid);
    if (loose.u.pole_count != 2 || loose.v.pole_count != 2 || !(deviation > 0.1 && deviation <= 1.0))
    {
        throw std::runtime_error("Expected a 2x2 fit with a measurable error within the loose tolerance");
    }
    const auto tight = approximate_surface_grid(grid, 0.1).value();
    if (tight.poles.size() <= loose.poles.size() || measured_error(tight, grid) > 0.1)
    {
        throw std::runtime_error("Tightening the tolerance did not refine the control net");
    }

    EditableNurbsSurface model(make_saddle_surface());
    model.preview_replacement(loose).value();
    const auto peak = model.displayed().max_grid_deviation.value();
    if (model.can_undo() || std::abs(peak.distance_mm - deviation) > 1e-12)
    {
        throw std::runtime_error("Preview did not measure all 12 original data points");
    }
    if (peak.u_index != 2 || peak.v_index != 1 || peak.data_point != grid[1][2].position ||
        std::abs(peak.distance_mm - 0.45) > 1e-12 || std::abs(peak.surface_point.z - 0.05) > 1e-12 ||
        std::hypot(peak.surface_point.x - peak.data_point.x, peak.surface_point.y - peak.data_point.y) > 1e-12)
    {
        throw std::runtime_error("The maximum deviation must connect the raised U3/V2 sample to its fitted position");
    }
    model.commit_preview();
    model.preview_pole_move(0, {100, 100, 100}).value();
    const auto edited = model.displayed().max_grid_deviation.value();
    if (edited.u_index != 0 || edited.v_index != 0 || edited.data_point != grid.front().front().position ||
        edited.surface_point != geometry::Vec3{100, 100, 100} || model.current().max_grid_deviation != peak)
    {
        throw std::runtime_error("Dragging a corner must move only the preview marker to U1/V1");
    }
    model.commit_preview();
    if (model.current().max_grid_deviation != edited || edited.distance_mm < 1.0)
    {
        throw std::runtime_error("Editing the fitted surface did not update its deviation");
    }
    model.undo();
    if (model.current().max_grid_deviation != peak)
    {
        throw std::runtime_error("Undo lost the original fit");
    }
    model.redo();
    const ProjectTestDirectory directory;
    const auto path = directory.path / "approximation.occtlab";
    document::save_nurbs_surface_project(path, model).value();
    const auto reopened = document::open_nurbs_surface_project(path).value();
    const auto &saved_grid = reopened.current().definition.data_grid.value();
    if (saved_grid.columns != 4 || saved_grid.rows != 3 || saved_grid.points.size() != 12 ||
        saved_grid.fit_tolerance_mm != 1.0 || reopened.current().definition != model.current().definition ||
        reopened.current().max_grid_deviation != edited)
    {
        throw std::runtime_error("Save/reopen lost the fitted surface, its source grid or its target tolerance");
    }

    for (const double invalid :
         {0.0, -1.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()})
    {
        if (approximate_surface_grid(grid, invalid))
        {
            throw std::runtime_error("Accepted an invalid point-grid fitting tolerance");
        }
    }
    grid.front().pop_back();
    if (approximate_surface_grid(grid, 1))
    {
        throw std::runtime_error("Accepted a ragged fitting grid");
    }
}
