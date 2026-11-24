import std;
import occt_lab.application.editable_curve;
import occt_lab.geometry.vec3;
import occt_lab.test.support.near;

namespace
{
using occt_lab::geometry::Vec3;
using occt_lab::test::near;

void check_pole_drag()
{
    EditableCurve curve;
    const auto original = curve.current();
    constexpr Vec3 intermediate{.x = 50.0, .y = -42.0, .z = 2.0};
    constexpr Vec3 final_position{.x = 72.0, .y = -42.0, .z = 4.0};
    if (!curve.preview_pole_move(0, intermediate) || !curve.preview_pole_move(0, final_position) ||
        !near(curve.displayed().samples.front(), final_position) || curve.current().definition != original.definition ||
        curve.displayed().framing_extent != original.framing_extent || curve.can_undo())
    {
        throw std::runtime_error("Dragging must preview the endpoint at fixed scale without changing history");
    }
    curve.accept_pole_move();
    if (!near(curve.current().samples.front(), final_position))
    {
        throw std::runtime_error("Release must apply the last preview");
    }
    curve.undo();
    if (curve.current().definition != original.definition)
    {
        throw std::runtime_error("One undo must restore the curve from before the drag");
    }
    if (!curve.preview_pole_move(0, intermediate))
    {
        throw std::runtime_error("Could not preview a new drag after undo");
    }
    curve.cancel_pole_move();
    if (curve.displayed().definition != original.definition || !curve.can_redo())
    {
        throw std::runtime_error("Cancelling a drag must restore the curve and retain redo");
    }
    auto invalid = intermediate;
    invalid.z = std::numeric_limits<double>::infinity();
    if (!curve.preview_pole_move(0, intermediate) || curve.preview_pole_move(0, invalid))
    {
        throw std::runtime_error("Invalid pole coordinates must fail after a valid preview");
    }
    curve.accept_pole_move();
    if (curve.current().definition != original.definition || !curve.can_redo())
    {
        throw std::runtime_error("A failed drag must discard the stale preview and preserve history");
    }
    if (!curve.preview_pole_move(0, intermediate) || !curve.preview_pole_move(0, original.definition.poles.front()))
    {
        throw std::runtime_error("Could not return the pole to its starting position");
    }
    curve.accept_pole_move();
    if (!curve.can_redo())
    {
        throw std::runtime_error("Returning to the drag origin must preserve redo");
    }
    curve.redo();
    if (!near(curve.current().samples.front(), final_position))
    {
        throw std::runtime_error("Redo must restore the released position");
    }
}

void check_curve_edits()
{
    EditableCurve curve;
    const auto original = curve.current();
    auto edited = original.definition;
    edited.poles.front().z = 18.0;
    edited.poles[2].y += 10.0;
    constexpr auto middle = original.samples.size() / 2;
    if (!curve.apply(edited) || curve.current().definition != edited ||
        !near(curve.current().samples.front(), edited.poles.front()) ||
        near(curve.current().samples[middle], original.samples[middle]))
    {
        throw std::runtime_error("An applied edit must replace the poles and sampled curve together");
    }
    const auto unit_samples = curve.current().samples;
    const auto unit_definition = edited;
    curve.undo();
    if (curve.current().definition != original.definition || curve.current().samples != original.samples)
    {
        throw std::runtime_error("Undo failed to restore the original control points and samples");
    }
    curve.redo();
    if (curve.current().definition != unit_definition || curve.current().samples != unit_samples)
    {
        throw std::runtime_error("Redo failed to restore the edited control points and samples");
    }

    edited.weights[2] = 4.0;
    if (!curve.apply(edited) || curve.current().definition != edited ||
        curve.current().unit_weight_samples != unit_samples ||
        near(curve.current().samples[middle], unit_samples[middle]))
    {
        throw std::runtime_error("A weight-only edit must retain the unit curve for the same poles");
    }
    const auto weighted_samples = curve.current().samples;
    curve.undo();
    if (!curve.can_redo() || curve.can_undo() || curve.current().definition != unit_definition ||
        curve.current().samples != unit_samples || curve.current().unit_weight_samples)
    {
        throw std::runtime_error("Undo failed to restore the unit-weight curve");
    }
    auto invalid = edited;
    invalid.poles[1].x = std::numeric_limits<double>::infinity();
    if (curve.apply(invalid))
    {
        throw std::runtime_error("An infinite pole coordinate was accepted");
    }
    invalid = edited;
    for (const double weight :
         {0.0, -1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
    {
        invalid.weights[2] = weight;
        if (curve.apply(invalid))
        {
            throw std::runtime_error("A non-positive or non-finite weight was accepted");
        }
    }
    if (!curve.apply(unit_definition) || !curve.can_redo() || curve.current().definition != unit_definition ||
        curve.current().samples != unit_samples || curve.current().unit_weight_samples)
    {
        throw std::runtime_error("Failed or unchanged edits must preserve the curve and redo entry");
    }
    curve.redo();
    if (curve.current().definition != edited || curve.current().samples != weighted_samples ||
        curve.current().unit_weight_samples != unit_samples)
    {
        throw std::runtime_error("Redo failed to restore the edited curve");
    }
    curve.undo();
    edited.poles.back().z = -5.0;
    if (!curve.apply(edited) || curve.can_redo() || !curve.can_undo())
    {
        throw std::runtime_error("A replacement edit must start a new history branch");
    }
}
} // namespace

void check_curve_editing()
{
    check_curve_edits();
    check_pole_drag();
}
