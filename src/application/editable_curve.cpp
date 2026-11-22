#include "editable_curve.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
#include <stdexcept>
#include <utility>

namespace
{
constexpr std::array<occt_lab::geometry::Vec3, 6> initial_poles{{
    {.x = 42.0, .y = -42.0, .z = 0.0},
    {.x = 5.0, .y = -38.0, .z = 0.0},
    {.x = -10.0, .y = -26.0, .z = 0.0},
    {.x = 10.0, .y = 26.0, .z = 0.0},
    {.x = -5.0, .y = 38.0, .z = 0.0},
    {.x = -42.0, .y = 42.0, .z = 0.0},
}};

double max_sample_deviation(const EditableCurve::Curve::Samples &before, const EditableCurve::Curve::Samples &after)
{
    double maximum = 0.0;
    for (std::size_t i = 0; i < before.size(); ++i)
    {
        const auto &[x0, y0, z0] = before[i];
        const auto &[x1, y1, z1] = after[i];
        const double distance = std::hypot(x1 - x0, y1 - y0, z1 - z0);
        if (!std::isfinite(distance))
        {
            throw std::runtime_error("The sampled deviation exceeds the finite coordinate range");
        }
        maximum = std::max(maximum, distance);
    }
    return maximum;
}
} // namespace

EditableCurve::EditableCurve() : history_(sample_curve(occt_lab::geometry::make_uniform_clamped_curve(initial_poles, 3)))
{
}

EditableCurve::EditableCurve(Curve curve) : history_(std::move(curve))
{
}

auto EditableCurve::current() const -> const Curve &
{
    return history_.current();
}

auto EditableCurve::displayed() const -> const Curve &
{
    return pole_preview_ ? *pole_preview_ : current();
}

auto EditableCurve::preview_pole_move(const std::size_t index, const occt_lab::geometry::Vec3 &position)
    -> std::expected<void, std::string>
{
    try
    {
        if (current().definition.poles.at(index) == position)
        {
            cancel_pole_move();
            return {};
        }
        if (displayed().definition.poles[index] != position)
        {
            auto definition = current().definition;
            definition.poles[index] = position;
            auto candidate = sample_curve(definition);
            // Keep the grabbed point under the cursor even when it becomes the new outermost pole.
            candidate.framing_extent = current().framing_extent;
            pole_preview_ = std::move(candidate);
        }
        return {};
    }
    catch (const std::exception &error)
    {
        cancel_pole_move();
        return std::unexpected(std::string{error.what()});
    }
}

void EditableCurve::accept_pole_move()
{
    if (pole_preview_)
    {
        commit(std::move(*pole_preview_));
    }
}

void EditableCurve::cancel_pole_move()
{
    pole_preview_.reset();
}

void EditableCurve::commit(Curve candidate)
{
    cancel_pole_move();
    history_.commit(std::move(candidate));
}

void EditableCurve::commit_refinement(const Definition &definition)
{
    const auto &before = current();
    auto candidate = sample_curve(definition, before.data_points);
    candidate.refinement = occt_lab::geometry::RefinementComparison{
        .previous_degree = before.definition.degree,
        .previous_pole_count = before.definition.poles.size(),
        .max_sample_deviation = max_sample_deviation(before.samples, candidate.samples),
    };
    candidate.framing_extent = before.framing_extent;
    candidate.approximation = before.approximation;
    commit(std::move(candidate));
}

auto EditableCurve::create_line(const LineCurveParameters params) -> std::expected<void, std::string>
{
    try
    {
        replace_with_created_curve(sample_curve(occt_lab::make_line_curve(params)));
        return {};
    }
    catch (const std::exception &error)
    {
        return std::unexpected(std::string{error.what()});
    }
}

auto EditableCurve::create_cubic_bezier() -> std::expected<void, std::string>
{
    try
    {
        replace_with_created_curve(sample_curve(occt_lab::make_cubic_bezier_curve()));
        return {};
    }
    catch (const std::exception &error)
    {
        return std::unexpected(std::string{error.what()});
    }
}

void EditableCurve::replace_with_created_curve(Curve candidate, const Framing framing)
{
    if (framing == Framing::keep_view)
    {
        candidate.framing_extent = current().framing_extent;
    }
    if (candidate == current())
    {
        cancel_pole_move();
        return;
    }
    commit(std::move(candidate));
}

std::expected<void, std::string> EditableCurve::apply(const Definition &definition)
{
    if (definition == current().definition)
    {
        return {};
    }
    try
    {
        commit(sample_curve(definition));
        return {};
    }
    catch (const std::exception &error)
    {
        return std::unexpected(std::string{error.what()});
    }
}

std::expected<void, std::string> EditableCurve::insert_knot(const double parameter)
{
    try
    {
        commit_refinement(insert_curve_knot(current().definition, parameter));
        return {};
    }
    catch (const std::exception &error)
    {
        return std::unexpected(std::string{error.what()});
    }
}

std::expected<void, std::string> EditableCurve::elevate_degree(const int target_degree)
{
    try
    {
        commit_refinement(elevate_curve_degree(current().definition, target_degree));
        return {};
    }
    catch (const std::exception &error)
    {
        return std::unexpected(std::string{error.what()});
    }
}

std::expected<void, std::string> EditableCurve::interpolate_through_poles()
{
    return create_interpolated_curve(current().definition.poles);
}

auto EditableCurve::create_interpolated_curve(const std::span<const occt_lab::geometry::Vec3> points,
                                              const Framing framing) -> std::expected<void, std::string>
{
    try
    {
        replace_with_created_curve(occt_lab::geometry::interpolate_curve(points), framing);
        return {};
    }
    catch (const std::exception &error)
    {
        return std::unexpected(std::string{error.what()});
    }
}

std::expected<void, std::string> EditableCurve::approximate_data_points(const double tolerance)
{
    const auto &curve = current();
    if (curve.approximation && !curve.refinement && curve.approximation->requested_tolerance == tolerance)
    {
        return {};
    }
    try
    {
        std::vector<occt_lab::geometry::Vec3> saved_positions;
        for (const auto &input : curve.data_points)
        {
            saved_positions.push_back(input.position);
        }
        const auto &positions = saved_positions.empty() ? curve.definition.poles : saved_positions;
        commit(occt_lab::geometry::approximate_curve(positions, tolerance));
        return {};
    }
    catch (const std::exception &error)
    {
        return std::unexpected(std::string{error.what()});
    }
}

void EditableCurve::undo()
{
    cancel_pole_move();
    history_.undo();
}

void EditableCurve::redo()
{
    cancel_pole_move();
    history_.redo();
}
