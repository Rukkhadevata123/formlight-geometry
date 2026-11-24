import std;
import occt_lab.application.editable_curve;
import occt_lab.geometry.curves.bspline_curve;
import occt_lab.test.support.near;

namespace
{
using occt_lab::geometry::Vec3;
using occt_lab::test::near;

void check_interpolation()
{
    constexpr std::array data{Vec3{.x = 0.0, .y = 0.0, .z = 0.0}, Vec3{.x = 1.0, .y = 0.0, .z = 0.0},
                              Vec3{.x = 1.0, .y = 2.0, .z = 0.0}, Vec3{.x = 1.0, .y = 2.0, .z = 1.0}};
    EditableCurve curve;
    auto source = occt_lab::geometry::make_uniform_clamped_curve(data, 3);
    source.weights[1] = 4.0;
    if (!curve.apply(source) || !curve.interpolate_through_poles())
    {
        throw std::runtime_error("Failed to interpolate four distinct 3D data points");
    }
    const auto interpolated = curve.current();
    if (interpolated.unit_weight_samples ||
        !std::ranges::equal(interpolated.data_points, data, {}, &occt_lab::geometry::CurveDataPoint::position))
    {
        throw std::runtime_error("Interpolation must use the applied pole positions and solve for its own curve");
    }
    curve.undo();
    if (curve.current().definition != source || !curve.current().data_points.empty())
    {
        throw std::runtime_error("Undo must restore the weighted source curve");
    }
    curve.redo();
    if (curve.current().data_points != interpolated.data_points ||
        curve.current().definition != interpolated.definition)
    {
        throw std::runtime_error("Redo must restore the interpolant and its residuals");
    }
    if (!curve.insert_knot(0.5) || curve.current().data_points.size() != data.size())
    {
        throw std::runtime_error("Knot insertion lost the interpolation data");
    }
    for (const auto &point : curve.current().data_points)
    {
        if (!near(point.residual, 0.0))
        {
            throw std::runtime_error("The refined curve no longer passes through the input points");
        }
    }
    auto edited = curve.current().definition;
    edited.poles.front().z += 1.0;
    if (!curve.apply(edited) || !curve.current().data_points.empty())
    {
        throw std::runtime_error("A pole edit must clear the previous interpolation report");
    }
}

void check_interpolation_failure()
{
    EditableCurve curve;
    const auto original = curve.current().definition;
    auto duplicate = original;
    duplicate.poles[1] = duplicate.poles.front();
    if (!curve.apply(duplicate) || !curve.apply(original))
    {
        throw std::runtime_error("Could not prepare a curve with coincident adjacent poles");
    }
    curve.undo();
    if (curve.interpolate_through_poles() || curve.approximate_data_points(1.0) ||
        curve.current().definition != duplicate || !curve.can_redo())
    {
        throw std::runtime_error("Failed point fitting must preserve the source curve and redo entry");
    }
}

void check_off_grid_residual()
{
    constexpr std::array data{Vec3{.x = 0.0, .y = 0.0, .z = 0.0}, Vec3{.x = 1.0, .y = 0.0, .z = 0.0},
                              Vec3{.x = 1.0, .y = 3.0, .z = 0.0}, Vec3{.x = 1.0, .y = 3.0, .z = 3.0}};
    const auto curve = occt_lab::geometry::interpolate_curve(data);
    const auto &input = curve.data_points.at(1);
    // u = 1/7 lies between the 129 display samples.
    if (!near(input.parameter, 1.0 / 7.0, 1e-14) || !near(input.residual, 0.0))
    {
        throw std::runtime_error("Residuals must be evaluated at the original chord parameters");
    }
}

void check_approximation()
{
    // Equal chord lengths put all nine data parameters on the display sampling grid.
    std::array<Vec3, 9> data{};
    for (std::size_t i = 0; i < data.size(); ++i)
    {
        data[i] = {.x = static_cast<double>(i), .y = static_cast<double>(i % 2), .z = 0.0};
    }
    EditableCurve curve;
    const auto source = occt_lab::geometry::make_uniform_clamped_curve(data, 3);
    constexpr double tolerance = 2.0;
    if (!curve.apply(source) || !curve.approximate_data_points(tolerance))
    {
        throw std::runtime_error("Could not approximate the applied pole positions");
    }
    const auto fit = curve.current();
    if (!fit.approximation || fit.data_points.size() != data.size() ||
        fit.approximation->interpolated_residuals.size() != data.size() ||
        fit.approximation->requested_tolerance != tolerance)
    {
        throw std::runtime_error("The fit must retain its tolerance and both sets of residuals");
    }
    for (std::size_t i = 0; i < data.size(); ++i)
    {
        const auto &[position, parameter, residual] = fit.data_points[i];
        const auto sample = i * (fit.samples.size() - 1) / (data.size() - 1);
        const auto &[x, y, z] = fit.samples[sample];
        const double measured = std::hypot(x - data[i].x, y - data[i].y, z - data[i].z);
        if (position != data[i] || !near(parameter, static_cast<double>(i) / 8.0) || !near(residual, measured) ||
            !near(fit.approximation->interpolated_samples[sample], data[i]) ||
            !near(fit.approximation->interpolated_residuals[i], 0.0))
        {
            throw std::runtime_error("Both fits must measure the same points at the same chord parameters");
        }
    }
    if (fit.samples == fit.approximation->interpolated_samples)
    {
        throw std::runtime_error("A loose cubic fit to zigzag data unexpectedly duplicated the interpolant");
    }
    if (!curve.approximate_data_points(tolerance))
    {
        throw std::runtime_error("Reapplying the same fit should preserve history");
    }
    curve.undo();
    for (const double invalid :
         {0.0, -1.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()})
    {
        if (curve.approximate_data_points(invalid) || curve.current().definition != source || !curve.can_redo())
        {
            throw std::runtime_error("Invalid approximation tolerance changed the curve or lost redo");
        }
    }
    curve.redo();
    if (curve.current().definition != fit.definition || curve.current().approximation != fit.approximation ||
        curve.current().data_points != fit.data_points)
    {
        throw std::runtime_error("Redo lost the approximation or its interpolation reference");
    }
    if (!curve.insert_knot(0.45) || curve.current().approximation != fit.approximation ||
        !curve.approximate_data_points(0.1) || curve.current().data_points.size() != data.size())
    {
        throw std::runtime_error("Refinement and refitting must retain the original data set");
    }
    for (std::size_t i = 0; i < data.size(); ++i)
    {
        if (curve.current().data_points[i].position != data[i])
        {
            throw std::runtime_error("Refitting used the approximation poles instead of the original data");
        }
    }
    if (!curve.apply(source) || curve.current().approximation || !curve.current().data_points.empty() ||
        !curve.interpolate_through_poles() || !curve.approximate_data_points(tolerance) ||
        curve.current().data_points != fit.data_points)
    {
        throw std::runtime_error("Comparison after interpolation must fit the original pole positions");
    }
}
} // namespace

void check_curve_fitting()
{
    check_interpolation();
    check_interpolation_failure();
    check_off_grid_residual();
    check_approximation();
}
