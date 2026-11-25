import std;
import formlight_geometry.application.editable_nurbs_surface;
import formlight_geometry.application.nurbs_surface_creation;
import formlight_geometry.document.nurbs_surface_project;
import formlight_geometry.geometry.surfaces.bspline_surface;
import formlight_geometry.test.support.project_test_directory;

namespace
{
using namespace formlight_geometry::document;

void check_rejected(const std::filesystem::path &path, std::string text, const std::string_view field,
                    const std::string_view replacement, const std::string_view expected_error = {})
{
    text.replace(text.find(field), field.size(), replacement);
    std::ofstream out(path, std::ios::binary);
    out.exceptions(std::ios::badbit | std::ios::failbit);
    out << text;
    out.close();
    const auto result = open_nurbs_surface_project(path);
    if (result)
    {
        throw std::runtime_error(std::format("Accepted invalid NURBS project field '{}'", field));
    }
    if (!result.error().contains(expected_error))
    {
        throw std::runtime_error(std::format("Unexpected rejection for '{}': {}", field, result.error()));
    }
}
} // namespace

void check_nurbs_surface_projects()
{
    const ProjectTestDirectory directory;
    const auto path = directory.path / u8"自定义曲面.occtlab";
    using KnotList = std::vector<formlight_geometry::geometry::BSplineKnot>;
    const formlight_geometry::SurfaceCreationParameters parameters{
        .u_poles = 6,
        .v_poles = 4,
        .u_degree = 2,
        .v_degree = 1,
        .u_knots = KnotList{{-2, 3}, {0, 2}, {3, 1}, {5, 3}},
        .v_knots = KnotList{{-4, 2}, {-1, 1}, {1, 1}, {4, 2}},
    };
    auto definition = formlight_geometry::make_planar_nurbs_surface(parameters).value();
    EditableNurbsSurface model{definition};
    save_nurbs_surface_project(path, model).value();

    definition.poles[9] = {11.123456789012345, -19.5, 32.25};
    definition.poles[14].z = -0.23456789012345678;
    definition.weights.assign(definition.poles.size(), 3);
    definition.weights[9] = 7.123456789012345;
    model.apply(definition.poles, definition.weights).value();
    model.preview_pole_move(9, {22, 0, 0}).value();
    save_nurbs_surface_project(path, model).value();

    auto loaded = open_nurbs_surface_project(path).value();
    if (loaded.current().definition != definition)
    {
        throw std::runtime_error("Reopening changed the committed NURBS definition or its original weight scale");
    }
    if (loaded.current().isolines.constant_u != model.current().isolines.constant_u ||
        loaded.current().isolines.constant_v != model.current().isolines.constant_v)
    {
        throw std::runtime_error("Isolines changed after reopening");
    }
    const formlight_geometry::geometry::BSplineSurface before{definition};
    const formlight_geometry::geometry::BSplineSurface after{loaded.current().definition};
    const auto a = before.evaluate_d1(1.25, 0.375).value();
    const auto b = after.evaluate_d1(1.25, 0.375).value();
    if (a.position != b.position || a.du != b.du || a.dv != b.dv)
    {
        throw std::runtime_error("Position or derivatives changed after reopening");
    }
    if (loaded.can_undo() || loaded.can_redo())
    {
        throw std::runtime_error("Opening should start a fresh NURBS edit history");
    }
    auto edited = definition.poles;
    edited[3].z = 12;
    loaded.apply(edited, definition.weights).value();
    loaded.undo();
    if (loaded.current().definition != definition || !loaded.can_redo())
    {
        throw std::runtime_error("Undo after reopening did not restore the saved surface");
    }

    std::ifstream in(path, std::ios::binary);
    const std::string text{std::istreambuf_iterator<char>{in}, {}};
    const auto invalid = directory.path / "invalid.occtlab";
    check_rejected(invalid, text, "occt-lab-nurbs-surface 2", "occt-lab-bezier-patch 1", "occt-lab-nurbs-surface");
    check_rejected(invalid, text, "surface 2", "surface 1", "Unsupported NURBS surface project version");
    check_rejected(invalid, text, "occt-lab-nurbs-surface", "occt-lab-curve");
    check_rejected(invalid, text, "surface 2", "surface 3");
    check_rejected(invalid, text, "isolines 9 9 33", "isolines 0 9 33");
    check_rejected(invalid, text, "units mm", "units inch");
    check_rejected(invalid, text, "V degree 1", "V degree 0");
    check_rejected(invalid, text, "poles 6 knots 4", "poles 6 knots 999999999");
    check_rejected(invalid, text, "-2 3\n0 2", "-2 3\n0 1");
    check_rejected(invalid, text, "-45 -36 0 3", "-45 -36 0 0");
    check_rejected(invalid, text, "-45 -36 0 3", "nan -36 0 3");
    check_rejected(invalid, text, "-45 -36 0 3", "");
    check_rejected(invalid, text, "end\n", "end\nextra");
}
