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

EditableCurve::EditableCurve() : current_(sample_curve(occt_lab::geometry::make_uniform_cubic(initial_poles)))
{
}

auto EditableCurve::current() const -> const Curve &
{
    return current_;
}

auto EditableCurve::displayed() const -> const Curve &
{
    return pole_preview_ ? *pole_preview_ : current_;
}

auto EditableCurve::preview_pole_move(const std::size_t index, const occt_lab::geometry::Vec3 &position)
    -> std::expected<void, std::string>
{
    try
    {
        if (current_.definition.poles.at(index) == position)
        {
            cancel_pole_move();
            return {};
        }
        if (displayed().definition.poles[index] != position)
        {
            auto definition = current_.definition;
            definition.poles[index] = position;
            auto candidate = sample_curve(definition);
            // Keep the grabbed point under the cursor even when it becomes the new outermost pole.
            candidate.framing_extent = current_.framing_extent;
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
    history_ = std::exchange(current_, std::move(candidate));
    undone_ = false;
}

void EditableCurve::commit_refinement(const Definition &definition)
{
    auto candidate = sample_curve(definition, current_.data_points);
    candidate.refinement = occt_lab::geometry::RefinementComparison{
        .previous_degree = current_.definition.degree,
        .previous_pole_count = current_.definition.poles.size(),
        .max_sample_deviation = max_sample_deviation(current_.samples, candidate.samples),
    };
    candidate.framing_extent = current_.framing_extent;
    candidate.approximation = current_.approximation;
    commit(std::move(candidate));
}

std::expected<void, std::string> EditableCurve::apply(const Definition &definition)
{
    if (definition == current_.definition)
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
        commit_refinement(insert_curve_knot(current_.definition, parameter));
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
        commit_refinement(elevate_curve_degree(current_.definition, target_degree));
        return {};
    }
    catch (const std::exception &error)
    {
        return std::unexpected(std::string{error.what()});
    }
}

std::expected<void, std::string> EditableCurve::interpolate_through_poles()
{
    try
    {
        commit(occt_lab::geometry::interpolate_curve(current_.definition.poles));
        return {};
    }
    catch (const std::exception &error)
    {
        return std::unexpected(std::string{error.what()});
    }
}

std::expected<void, std::string> EditableCurve::approximate_data_points(const double tolerance)
{
    if (current_.approximation && !current_.refinement && current_.approximation->requested_tolerance == tolerance)
    {
        return {};
    }
    try
    {
        std::vector<occt_lab::geometry::Vec3> saved_positions;
        for (const auto &input : current_.data_points)
        {
            saved_positions.push_back(input.position);
        }
        const auto &positions = saved_positions.empty() ? current_.definition.poles : saved_positions;
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
    if (can_undo())
    {
        std::swap(current_, *history_);
        undone_ = true;
    }
}

void EditableCurve::redo()
{
    cancel_pole_move();
    if (can_redo())
    {
        std::swap(current_, *history_);
        undone_ = false;
    }
}
