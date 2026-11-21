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

EditableNurbsSurface::EditableNurbsSurface(Definition definition) : history_(sample_surface(std::move(definition)))
{
}

auto EditableNurbsSurface::apply(const std::vector<occt_lab::geometry::Vec3> &poles, const std::vector<double> &weights)
    -> std::expected<void, std::string>
{
    const auto &applied = current().definition;
    if (poles.size() != applied.poles.size() || weights.size() != poles.size())
    {
        return std::unexpected("Each control point requires one position and one weight.");
    }
    if (poles == applied.poles && weights == applied.weights)
    {
        discard_preview();
        return {};
    }
    try
    {
        commit(sample_surface({.u = applied.u, .v = applied.v, .poles = poles, .weights = weights}));
        return {};
    }
    catch (const std::exception &error)
    {
        return std::unexpected(error.what());
    }
}

auto EditableNurbsSurface::preview_pole_move(const std::size_t index, const occt_lab::geometry::Vec3 position)
    -> std::expected<void, std::string>
{
    try
    {
        if (position == current().definition.poles.at(index))
        {
            discard_preview();
        }
        else if (position != displayed().definition.poles[index])
        {
            auto definition = current().definition;
            definition.poles[index] = position;
            preview_ = sample_surface(std::move(definition));
        }
        return {};
    }
    catch (const std::exception &error)
    {
        discard_preview();
        return std::unexpected(error.what());
    }
}

void EditableNurbsSurface::commit(Snapshot next)
{
    history_.commit(std::move(next));
    discard_preview();
}

void EditableNurbsSurface::commit_preview()
{
    if (preview_)
    {
        commit(std::move(*preview_));
    }
}

void EditableNurbsSurface::discard_preview()
{
    preview_.reset();
}

void EditableNurbsSurface::undo()
{
    discard_preview();
    history_.undo();
}

void EditableNurbsSurface::redo()
{
    discard_preview();
    history_.redo();
}
