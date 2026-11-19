#include "editable_curve.hpp"

#include <exception>
#include <utility>

namespace
{
EditableCurve::Curve sampled_curve(const EditableCurve::Curve::Poles &poles)
{
    EditableCurve::Curve curve{.poles = poles, .samples = {}};
    occt_lab::geometry::sample_clamped_cubic_bspline(curve.poles, curve.samples);
    return curve;
}
constexpr EditableCurve::Curve::Poles initial_poles{{
    {.x = 42.0, .y = -42.0, .z = 0.0},
    {.x = 5.0, .y = -38.0, .z = 0.0},
    {.x = -10.0, .y = -26.0, .z = 0.0},
    {.x = 10.0, .y = 26.0, .z = 0.0},
    {.x = -5.0, .y = 38.0, .z = 0.0},
    {.x = -42.0, .y = 42.0, .z = 0.0},
}};
} // namespace

EditableCurve::EditableCurve() : current_(sampled_curve(initial_poles))
{
}

std::expected<void, std::string> EditableCurve::apply(const Curve::Poles &poles)
{
    if (poles == current_.poles)
    {
        return {};
    }
    try
    {
        history_ = std::exchange(current_, sampled_curve(poles));
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
