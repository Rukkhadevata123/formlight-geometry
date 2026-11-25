import std;
import formlight_geometry.application.editable_curve;
import formlight_geometry.geometry.curves.bspline_curve;
import formlight_geometry.test.support.near;

namespace
{
using formlight_geometry::geometry::Vec3;
using formlight_geometry::test::near;

void check_curve_samples()
{
    using formlight_geometry::geometry::make_uniform_clamped_curve;
    constexpr std::array poles{
        Vec3{.x = -3.0, .y = 1.0, .z = 0.0}, Vec3{.x = -2.0, .y = 2.0, .z = 1.0}, Vec3{.x = 1.0, .y = 4.0, .z = 3.0},
        Vec3{.x = 3.0, .y = -2.0, .z = 1.0}, Vec3{.x = 6.0, .y = 0.0, .z = 0.0},
    };
    const auto samples = sample_curve(make_uniform_clamped_curve(poles, 3)).samples;
    const auto middle = samples.size() / 2;

    if (!near(samples.front(), poles.front()) || !near(samples.back(), poles.back()))
    {
        throw std::runtime_error("The clamped curve must interpolate both endpoint poles");
    }
    // At the interior knot, the three active basis functions are 1/4, 1/2 and 1/4.
    if (constexpr Vec3 expected_middle{.x = 0.75, .y = 2.0, .z = 2.0}; !near(samples[middle], expected_middle))
    {
        throw std::runtime_error("Unexpected cubic B-spline sample at the middle knot");
    }

    constexpr std::array unit_weights{1.0, 1.0, 1.0, 1.0, 1.0};
    const auto unit = sample_curve(make_uniform_clamped_curve(poles, 3, unit_weights)).samples;
    if (unit != samples)
    {
        throw std::runtime_error("Unit weights must reproduce the polynomial B-spline");
    }

    constexpr std::array weights{1.0, 1.0, 3.0, 1.0, 1.0};
    const auto weighted = sample_curve(make_uniform_clamped_curve(poles, 3, weights)).samples;
    if (constexpr Vec3 rational_middle{.x = 0.875, .y = 3.0, .z = 2.5}; !near(weighted[middle], rational_middle) ||
                                                                        !near(weighted.front(), poles.front()) ||
                                                                        !near(weighted.back(), poles.back()))
    {
        throw std::runtime_error("Unexpected rational midpoint or clamped endpoints");
    }

    constexpr std::array scaled_weights{2.0, 2.0, 6.0, 2.0, 2.0};
    const auto scaled = sample_curve(make_uniform_clamped_curve(poles, 3, scaled_weights)).samples;
    for (std::size_t i = 0; i < samples.size(); ++i)
    {
        if (!near(scaled[i], weighted[i]))
        {
            throw std::runtime_error("Common scaling of all weights changed the rational curve");
        }
    }
}

void check_knot_insertion(const bool rational)
{
    EditableCurve curve;
    auto definition = curve.current().definition;
    definition.weights.assign(definition.poles.size(), 2.0);
    if (rational)
    {
        definition.weights[2] = 8.0;
    }
    if (!curve.apply(definition))
    {
        throw std::runtime_error("Failed to prepare the knot insertion fixture");
    }
    const auto original = curve.current();
    constexpr double parameter = 0.25;
    if (!curve.insert_knot(parameter))
    {
        throw std::runtime_error("An interior knot could not be inserted");
    }
    const auto refined = curve.current();
    const auto &knots = refined.definition.knots;
    if (refined.definition.poles.size() != original.definition.poles.size() + 1 ||
        knots.size() != original.definition.knots.size() + 1 || knots[1].value != parameter ||
        knots[1].multiplicity != 1 || knots.front() != definition.knots.front() ||
        knots.back() != definition.knots.back())
    {
        throw std::runtime_error("Insertion must add one simple interior knot and one pole");
    }
    if (!refined.refinement || !near(refined.refinement->max_sample_deviation, 0.0) ||
        refined.framing_extent != original.framing_extent || refined.unit_weight_samples.has_value() != rational ||
        refined.definition.weights.front() != definition.weights.front())
    {
        throw std::runtime_error("Insertion changed the sampled shape, framing or weight scale");
    }
    for (std::size_t i = 0; i < original.samples.size(); ++i)
    {
        if (!near(original.samples[i], refined.samples[i]))
        {
            throw std::runtime_error("The refined curve moved at a shared parameter");
        }
    }

    curve.undo();
    for (const double invalid : {0.0, 1.0, -0.25, 1.25, std::numeric_limits<double>::quiet_NaN()})
    {
        if (curve.insert_knot(invalid) || !curve.can_redo() || curve.current().definition != definition)
        {
            throw std::runtime_error("Invalid insertion must preserve the original definition and redo entry");
        }
    }
    curve.redo();
    if (curve.current().definition != refined.definition || curve.current().samples != refined.samples ||
        curve.current().refinement != refined.refinement)
    {
        throw std::runtime_error("Redo must restore the complete refined curve");
    }

    for (int multiplicity = 2; multiplicity <= 3; ++multiplicity)
    {
        if (!curve.insert_knot(parameter) || curve.current().definition.knots.size() != knots.size() ||
            curve.current().definition.knots[1].multiplicity != multiplicity)
        {
            throw std::runtime_error("Inserting an existing knot must increase its multiplicity");
        }
    }
    const auto saturated = curve.current().definition;
    if (curve.insert_knot(parameter) || curve.current().definition != saturated)
    {
        throw std::runtime_error("Inserting a saturated knot must preserve the cubic definition");
    }
    auto edited = saturated;
    edited.poles.back().z += 5.0;
    if (!curve.apply(edited) || curve.current().definition.knots != saturated.knots ||
        !near(curve.current().samples.back(), edited.poles.back()) || curve.current().refinement)
    {
        throw std::runtime_error("Pole editing must use the refined knots and clear the old deviation");
    }
}

void check_degree_elevation(const bool rational)
{
    EditableCurve curve;
    if (rational)
    {
        auto weighted = curve.current().definition;
        weighted.weights.assign(weighted.poles.size(), 2.0);
        weighted.weights[2] = 8.0;
        if (!curve.apply(weighted) || !curve.insert_knot(0.25))
        {
            throw std::runtime_error("Failed to prepare the nonuniform rational curve");
        }
    }

    const auto original = curve.current();
    if (!curve.elevate_degree(4))
    {
        throw std::runtime_error("Failed to elevate the cubic curve to degree four");
    }
    const auto elevated = curve.current();
    const auto &before = original.definition;
    const auto &after = elevated.definition;
    const auto expected_poles = before.poles.size() + before.knots.size() - 1;
    if (after.degree != 4 || after.poles.size() != expected_poles || after.knots.size() != before.knots.size())
    {
        throw std::runtime_error("Unexpected degree, pole count or knot count after elevation");
    }
    for (std::size_t i = 0; i < before.knots.size(); ++i)
    {
        if (after.knots[i].value != before.knots[i].value ||
            after.knots[i].multiplicity != before.knots[i].multiplicity + 1)
        {
            throw std::runtime_error("Degree elevation must preserve knot values and increase each multiplicity");
        }
    }
    if (!elevated.refinement || elevated.refinement->previous_degree != before.degree ||
        elevated.refinement->previous_pole_count != before.poles.size() ||
        !near(elevated.refinement->max_sample_deviation, 0.0) || elevated.framing_extent != original.framing_extent ||
        elevated.unit_weight_samples.has_value() != rational || !near(after.weights.front(), before.weights.front()))
    {
        throw std::runtime_error("Degree elevation lost the comparison, framing or weight scale");
    }
    for (std::size_t i = 0; i < original.samples.size(); ++i)
    {
        if (!near(elevated.samples[i], original.samples[i]))
        {
            throw std::runtime_error("Degree elevation changed a position at a shared parameter");
        }
    }

    curve.undo();
    for (const int invalid : {2, 3, std::numeric_limits<int>::max()})
    {
        if (curve.elevate_degree(invalid) || !curve.can_redo() || curve.current().definition != before ||
            curve.current().refinement != original.refinement)
        {
            throw std::runtime_error("Invalid elevation changed the curve or discarded redo");
        }
    }
    curve.redo();
    if (curve.current().definition != after || curve.current().samples != elevated.samples ||
        curve.current().refinement != elevated.refinement)
    {
        throw std::runtime_error("Redo must restore the elevated curve and its comparison");
    }
    const auto knot = after.knots[1];
    for (int multiplicity = knot.multiplicity + 1; multiplicity <= after.degree; ++multiplicity)
    {
        if (!curve.insert_knot(knot.value) || curve.current().definition.knots[1].multiplicity != multiplicity)
        {
            throw std::runtime_error("Knot insertion must use the elevated degree as its multiplicity limit");
        }
    }
    if (curve.insert_knot(knot.value) || !curve.elevate_degree(5))
    {
        throw std::runtime_error("The saturated quartic curve must reject another insertion and allow elevation");
    }
    auto edit = curve.current().definition;
    edit.poles.back().z += 5.0;
    if (!curve.apply(edit) || curve.current().definition.degree != 5 ||
        !near(curve.current().samples.back(), edit.poles.back()) || curve.current().refinement)
    {
        throw std::runtime_error("Pole edits must use the elevated degree and clear the comparison");
    }
}
} // namespace

void check_curve_geometry()
{
    check_curve_samples();
    check_knot_insertion(false);
    check_knot_insertion(true);
    check_degree_elevation(false);
    check_degree_elevation(true);
}
