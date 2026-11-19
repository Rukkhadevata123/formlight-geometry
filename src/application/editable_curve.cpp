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
            throw std::runtime_error("The sampled insertion error exceeds the finite coordinate range");
        }
        maximum = std::max(maximum, distance);
    }
    return maximum;
}
} // namespace

EditableCurve::EditableCurve() : current_(sample_curve(occt_lab::geometry::make_uniform_cubic(initial_poles)))
{
}

void EditableCurve::commit(Curve candidate)
{
    history_ = std::exchange(current_, std::move(candidate));
    undone_ = false;
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
        auto candidate = sample_curve(insert_cubic_knot(current_.definition, parameter));
        candidate.insertion_sample_error = max_sample_deviation(current_.samples, candidate.samples);
        candidate.framing_extent = current_.framing_extent;
        commit(std::move(candidate));
        return {};
    }
    catch (const std::exception &error)
    {
        return std::unexpected(std::string{error.what()});
    }
}

void EditableCurve::undo()
{
    if (can_undo())
    {
        std::swap(current_, *history_);
        undone_ = true;
    }
}

void EditableCurve::redo()
{
    if (can_redo())
    {
        std::swap(current_, *history_);
        undone_ = false;
    }
}
