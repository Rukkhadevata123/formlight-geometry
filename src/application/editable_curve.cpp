#include "editable_curve.hpp"

#include <algorithm>
#include <exception>
#include <utility>

namespace
{
EditableCurve::Curve sampled_curve(const EditableCurve::Definition &definition)
{
    EditableCurve::Curve curve{.definition = definition, .samples = {}, .unit_weight_samples = {}};
    const auto &[poles, weights] = definition;
    occt_lab::geometry::sample_clamped_cubic_bspline(poles, curve.samples, weights);
    if (!std::ranges::all_of(weights, [&weights](const double weight) { return weight == weights.front(); }))
    {
        occt_lab::geometry::sample_clamped_cubic_bspline(poles, curve.unit_weight_samples.emplace());
    }
    return curve;
}
constexpr EditableCurve::Definition::Poles initial_poles{{
    {.x = 42.0, .y = -42.0, .z = 0.0},
    {.x = 5.0, .y = -38.0, .z = 0.0},
    {.x = -10.0, .y = -26.0, .z = 0.0},
    {.x = 10.0, .y = 26.0, .z = 0.0},
    {.x = -5.0, .y = 38.0, .z = 0.0},
    {.x = -42.0, .y = 42.0, .z = 0.0},
}};
} // namespace

EditableCurve::EditableCurve() : current_(sampled_curve({.poles = initial_poles}))
{
}

std::expected<void, std::string> EditableCurve::apply(const Definition &definition)
{
    if (definition == current_.definition)
    {
        return {};
    }
    try
    {
        history_ = std::exchange(current_, sampled_curve(definition));
        undone_ = false;
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
