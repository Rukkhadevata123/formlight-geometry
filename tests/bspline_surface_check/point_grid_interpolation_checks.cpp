import std;
import occt_lab.application.editable_nurbs_surface;
import occt_lab.application.nurbs_surface_creation;
import occt_lab.document.nurbs_surface_project;
import occt_lab.geometry.surfaces.bspline_surface;
import occt_lab.test.support.project_test_directory;

namespace
{
occt_lab::SurfacePoleRows check_grid(const int columns, const int rows)
{
    occt_lab::SurfacePoleRows grid;
    occt_lab::resize_surface_net(grid, columns, rows);
    for (int v = 0; v < rows; ++v)
    {
        for (int u = 0; u < columns; ++u)
        {
            grid[v][u].position = {4.0 * u + v * v, 7.0 * v - u, std::sin(u + 2.0 * v) * 12};
        }
    }
    const auto def = occt_lab::interpolate_surface_grid(grid).value();
    const occt_lab::geometry::BSplineSurface surface{def};
    if (def.u.degree != std::min(3, columns - 1) || def.v.degree != std::min(3, rows - 1))
    {
        throw std::runtime_error("Unexpected interpolation degrees");
    }
    for (int v = 0; v < rows; ++v)
    {
        for (int u = 0; u < columns; ++u)
        {
            const auto p =
                surface.position(static_cast<double>(u) / (columns - 1), static_cast<double>(v) / (rows - 1)).value();
            const auto q = grid[v][u].position;
            if (!(std::hypot(p.x - q.x, p.y - q.y, p.z - q.z) < 1e-9))
            {
                throw std::runtime_error(std::format("Missed interpolation point at U {}, V {}", u, v));
            }
        }
    }
    return grid;
}
} // namespace

void check_point_grid_interpolation()
{
    using namespace occt_lab;
    check_grid(2, 2);
    check_grid(2, 8);
    check_grid(7, 5);
    check_grid(SurfaceCreationParameters::max_poles, 3);
    auto grid = check_grid(4, 3);
    const auto def = interpolate_surface_grid(grid).value();
    if (def.poles == def.data_grid->points)
    {
        throw std::runtime_error("The curved 4x3 data grid was treated as a control net");
    }
    EditableNurbsSurface model{make_saddle_surface()};
    model.preview_replacement(def).value();
    if (!(model.displayed().max_data_point_error_mm.value() < 1e-9) || model.can_undo())
    {
        throw std::runtime_error("Bad point-grid preview or interpolation error");
    }
    model.discard_preview();
    if (model.displayed().max_data_point_error_mm)
    {
        throw std::runtime_error("Cancelled interpolation left a stale point grid");
    }
    model.replace(def).value();
    model.undo();
    if (model.current().max_data_point_error_mm)
    {
        throw std::runtime_error("Undo did not restore the original surface");
    }
    model.redo();
    auto poles = model.current().definition.poles;
    poles.front().z += 4;
    model.apply(poles, model.current().definition.weights).value();
    if (std::abs(model.current().max_data_point_error_mm.value() - 4) > 1e-9)
    {
        throw std::runtime_error("Editing a pole did not update the deviation from the original grid");
    }
    const ProjectTestDirectory directory;
    const auto path = directory.path / "point-grid.occtlab";
    document::save_nurbs_surface_project(path, model).value();
    const auto loaded = document::open_nurbs_surface_project(path).value();
    if (loaded.current().definition != model.current().definition ||
        loaded.current().max_data_point_error_mm != model.current().max_data_point_error_mm)
    {
        throw std::runtime_error("Reopening lost the interpolation grid or changed its deviation");
    }
    grid[1][2].position.z = std::numeric_limits<double>::infinity();
    const auto invalid = interpolate_surface_grid(grid);
    if (invalid || !invalid.error().contains("U 3, V 2"))
    {
        throw std::runtime_error("Non-finite interpolation point was not identified");
    }
    grid[1].pop_back();
    if (interpolate_surface_grid(grid) || interpolate_surface_grid({}))
    {
        throw std::runtime_error("Accepted an incomplete interpolation grid");
    }
}
