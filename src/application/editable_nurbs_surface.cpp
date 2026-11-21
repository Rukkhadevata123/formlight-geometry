#include "editable_nurbs_surface.hpp"

#include <stdexcept>
#include <utility>

namespace
{
EditableNurbsSurface::Snapshot sample_surface(EditableNurbsSurface::Definition definition)
{
    if (definition.weights.empty())
    {
        definition.weights.assign(definition.poles.size(), 1.0);
    }
    const occt_lab::geometry::BSplineSurface surface{definition};
    auto lines = surface.isolines();
    if (!lines)
    {
        throw std::runtime_error(lines.error());
    }
    return {.definition = std::move(definition), .isolines = std::move(*lines)};
}
} // namespace

EditableNurbsSurface::EditableNurbsSurface(Definition definition) : current_(sample_surface(std::move(definition)))
{
}

auto EditableNurbsSurface::apply(const std::vector<occt_lab::geometry::Vec3> &poles, const std::vector<double> &weights)
    -> std::expected<void, std::string>
{
    const auto &applied = current_.definition;
    if (poles.size() != applied.poles.size() || weights.size() != poles.size())
    {
        return std::unexpected("Each control point requires one position and one weight.");
    }
    if (poles == applied.poles && weights == applied.weights)
    {
        return {};
    }
    try
    {
        auto next = sample_surface({.u = applied.u, .v = applied.v, .poles = poles, .weights = weights});
        history_ = std::exchange(current_, std::move(next));
        undone_ = false;
        return {};
    }
    catch (const std::exception &error)
    {
        return std::unexpected(error.what());
    }
}

void EditableNurbsSurface::undo()
{
    if (can_undo())
    {
        std::swap(current_, *history_);
        undone_ = true;
    }
}

void EditableNurbsSurface::redo()
{
    if (can_redo())
    {
        std::swap(current_, *history_);
        undone_ = false;
    }
}
