import std;
import formlight_geometry.application.editable_nurbs_surface;
import formlight_geometry.document.nurbs_surface_project;
import formlight_geometry.geometry.surfaces.bspline_surface;
import formlight_geometry.test.bspline_surface_check.analytic_surface;
import formlight_geometry.test.support.project_test_directory;

namespace
{
using formlight_geometry::geometry::PatchSampling;

void check_lines(const EditableNurbsSurface::Snapshot &snapshot, const PatchSampling sampling)
{
    const auto expected = formlight_geometry::geometry::BSplineSurface{snapshot.definition}.isolines(sampling).value();
    if (snapshot.isolines.constant_u != expected.constant_u || snapshot.isolines.constant_v != expected.constant_v)
    {
        throw std::runtime_error("NURBS isolines do not match the snapshot at the selected density");
    }
}
} // namespace

void check_nurbs_resampling()
{
    const auto original = formlight_geometry::test::make_analytic_surface();
    EditableNurbsSurface model{original};
    const auto frame = model.current().framing;
    constexpr PatchSampling coarse{.constant_u_lines = 2, .constant_v_lines = 7, .samples_per_line = 2};
    model.resample(coarse).value();
    if (model.can_undo() || model.can_redo() || model.current().definition != original ||
        model.current().framing != frame)
    {
        throw std::runtime_error("Resampling alone changed NURBS geometry, framing or history");
    }
    check_lines(model.current(), coarse);

    auto edited = original;
    edited.weights.at(6) = 2.5;
    model.apply(edited.poles, edited.weights).value();
    check_lines(model.current(), coarse);
    constexpr PatchSampling fine{.constant_u_lines = 13, .constant_v_lines = 3, .samples_per_line = 41};
    model.resample(fine).value();
    model.undo();
    if (model.current().definition != original || !model.can_redo() || model.sampling() != fine)
    {
        throw std::runtime_error("Changing isoline density replaced the undo step");
    }
    check_lines(model.current(), fine);

    auto dragged = original.poles.at(6);
    dragged.z += 4;
    model.preview_pole_move(6, dragged).value();
    constexpr PatchSampling final_density{.constant_u_lines = 8, .constant_v_lines = 11, .samples_per_line = 23};
    model.resample(final_density).value();
    if (model.resample({.constant_v_lines = 1}) || model.sampling() != final_density || !model.can_redo() ||
        model.displayed().definition.poles.at(6) != dragged || model.current().definition != original)
    {
        throw std::runtime_error("An invalid sampling request disturbed the preview or redo history");
    }
    check_lines(model.current(), final_density);
    check_lines(model.displayed(), final_density);

    const ProjectTestDirectory directory;
    const auto path = directory.path / "resampled.occtlab";
    formlight_geometry::document::save_nurbs_surface_project(path, model).value();
    const auto restored = formlight_geometry::document::open_nurbs_surface_project(path).value();
    if (restored.sampling() != final_density || restored.current().definition != original)
    {
        throw std::runtime_error("Reopening lost the selected density or saved an unapplied preview");
    }
    check_lines(restored.current(), final_density);
    model.discard_preview();
    model.redo();
    if (model.current().definition != edited || model.sampling() != final_density)
    {
        throw std::runtime_error("Redo restored an outdated isoline density");
    }
    check_lines(model.current(), final_density);
}
