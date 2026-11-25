module;

#include <Bnd_Box.hxx>

export module formlight_geometry.application.editable_nurbs_surface;

import std;
import formlight_geometry.application.single_step_history;
import formlight_geometry.geometry.surfaces.bspline_surface;

export class EditableNurbsSurface
{
  public:
    using Definition = formlight_geometry::geometry::BSplineSurfaceDefinition;

    struct Framing
    {
        formlight_geometry::geometry::Vec3 center;
        double span;
        [[nodiscard]] double world_units_per_view_unit() const
        {
            return span > 0.0 ? span / 90.0 : 1.0;
        }
        bool operator==(const Framing &) const = default;
    };

    struct Snapshot
    {
        Definition definition;
        std::shared_ptr<const formlight_geometry::geometry::BSplineSurface> surface;
        formlight_geometry::geometry::PatchIsolines isolines;
        Framing framing;
        std::optional<formlight_geometry::geometry::SurfaceGridDeviation> max_grid_deviation;
    };

    explicit EditableNurbsSurface(Definition definition, formlight_geometry::geometry::PatchSampling sampling = {});
    [[nodiscard]] formlight_geometry::geometry::PatchSampling sampling() const
    {
        return sampling_;
    }
    [[nodiscard]] auto replace(Definition definition) -> std::expected<void, std::string>;
    [[nodiscard]] auto insert_knot(formlight_geometry::geometry::SurfaceDirection direction, double parameter)
        -> std::expected<void, std::string>;
    [[nodiscard]] auto elevate_degree(formlight_geometry::geometry::SurfaceDirection direction)
        -> std::expected<void, std::string>;
    [[nodiscard]] auto preview_replacement(Definition definition, std::optional<Framing> framing = {})
        -> std::expected<void, std::string>;
    [[nodiscard]] auto resample(formlight_geometry::geometry::PatchSampling sampling) -> std::expected<void, std::string>;
    [[nodiscard]] const Snapshot &current() const
    {
        return history_.current();
    }
    [[nodiscard]] const Snapshot &displayed() const
    {
        return preview_ ? *preview_ : current();
    }
    [[nodiscard]] bool can_undo() const
    {
        return history_.can_undo();
    }
    [[nodiscard]] bool can_redo() const
    {
        return history_.can_redo();
    }

    [[nodiscard]] auto apply(const std::vector<formlight_geometry::geometry::Vec3> &poles, const std::vector<double> &weights)
        -> std::expected<void, std::string>;
    [[nodiscard]] auto preview_pole_move(std::size_t index, formlight_geometry::geometry::Vec3 position)
        -> std::expected<void, std::string>;
    void commit_preview();
    void discard_preview();
    void undo();
    void redo();

  private:
    void commit(Snapshot next);
    formlight_geometry::geometry::PatchSampling sampling_;
    SingleStepHistory<Snapshot> history_;
    std::optional<Snapshot> preview_;
};

namespace
{
EditableNurbsSurface::Framing frame_control_net(const std::vector<formlight_geometry::geometry::Vec3> &poles)
{
    Bnd_Box bounds;
    for (const auto &[x, y, z] : poles)
    {
        bounds.Add(gp_Pnt{x, y, z});
    }
    const auto low = bounds.CornerMin();
    const auto high = bounds.CornerMax();
    return {.center = {std::midpoint(low.X(), high.X()), std::midpoint(low.Y(), high.Y()),
                       std::midpoint(low.Z(), high.Z())},
            .span = std::max({high.X() - low.X(), high.Y() - low.Y(), high.Z() - low.Z()})};
}

formlight_geometry::geometry::PatchIsolines sample_isolines(const formlight_geometry::geometry::BSplineSurface &surface,
                                                  const formlight_geometry::geometry::PatchSampling sampling)
{
    auto lines = surface.isolines(sampling);
    if (!lines)
    {
        throw std::runtime_error(lines.error());
    }
    return std::move(*lines);
}


EditableNurbsSurface::Snapshot sample_surface(EditableNurbsSurface::Definition definition,
                                              const formlight_geometry::geometry::PatchSampling sampling,
                                              std::optional<EditableNurbsSurface::Framing> framing = {})
{
    if (definition.weights.empty())
    {
        definition.weights.assign(definition.poles.size(), 1.0);
    }
    if (const auto &grid = definition.data_grid; grid && grid->fit_tolerance_mm)
    {
        if (!std::isfinite(*grid->fit_tolerance_mm) || *grid->fit_tolerance_mm <= 0)
        {
            throw std::invalid_argument("A fitting tolerance must be finite and positive.");
        }
    }
    auto surface = std::make_shared<const formlight_geometry::geometry::BSplineSurface>(definition);
    const auto deviation =
        definition.data_grid.transform([&](const auto &grid) { return surface->max_grid_deviation(grid); });
    auto lines = sample_isolines(*surface, sampling);
    if (!framing)
    {
        framing = frame_control_net(definition.poles);
    }
    return {.definition = std::move(definition), .surface = std::move(surface), .isolines = std::move(lines),
            .framing = *framing, .max_grid_deviation = deviation};
}
} // namespace

EditableNurbsSurface::EditableNurbsSurface(Definition definition, const formlight_geometry::geometry::PatchSampling sampling)
    : sampling_(sampling), history_(sample_surface(std::move(definition), sampling))
{
}

auto EditableNurbsSurface::replace(Definition definition) -> std::expected<void, std::string>
{
    return preview_replacement(std::move(definition)).transform([this] { commit_preview(); });
}

auto EditableNurbsSurface::insert_knot(const formlight_geometry::geometry::SurfaceDirection direction, const double parameter)
    -> std::expected<void, std::string>
{
    try
    {
        auto refined = insert_surface_knot(current().definition, direction, parameter);
        commit(sample_surface(std::move(refined), sampling_, current().framing));
        return {};
    }
    catch (const std::exception &error)
    {
        return std::unexpected(error.what());
    }
}

auto EditableNurbsSurface::elevate_degree(const formlight_geometry::geometry::SurfaceDirection direction)
    -> std::expected<void, std::string>
{
    try
    {
        auto elevated = elevate_surface_degree(current().definition, direction);
        commit(sample_surface(std::move(elevated), sampling_, current().framing));
        return {};
    }
    catch (const std::exception &error)
    {
        return std::unexpected(error.what());
    }
}

auto EditableNurbsSurface::preview_replacement(Definition definition, std::optional<Framing> framing)
    -> std::expected<void, std::string>
{
    try
    {
        auto next = sample_surface(std::move(definition), sampling_, framing);
        if (next.definition != current().definition || next.framing != current().framing)
        {
            preview_ = std::move(next);
        }
        else
        {
            discard_preview();
        }
        return {};
    }
    catch (const std::exception &error)
    {
        return std::unexpected(error.what());
    }
}

auto EditableNurbsSurface::resample(const formlight_geometry::geometry::PatchSampling sampling)
    -> std::expected<void, std::string>
{
    if (sampling == sampling_)
    {
        return {};
    }
    try
    {
        auto [applied, alternate] = history_.stored_snapshots();
        auto applied_lines = sample_isolines(*applied.surface, sampling);
        formlight_geometry::geometry::PatchIsolines alternate_lines;
        if (alternate)
        {
            alternate_lines = sample_isolines(*alternate->surface, sampling);
        }
        // Publish the caches only after every geometry has sampled successfully.
        if (preview_)
        {
            preview_->isolines = sample_isolines(*preview_->surface, sampling);
        }
        if (alternate)
        {
            alternate->isolines = std::move(alternate_lines);
        }
        applied.isolines = std::move(applied_lines);
        sampling_ = sampling;
        return {};
    }
    catch (const std::exception &error)
    {
        return std::unexpected(error.what());
    }
}

auto EditableNurbsSurface::apply(const std::vector<formlight_geometry::geometry::Vec3> &poles, const std::vector<double> &weights)
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
        commit(sample_surface(
            {.u = applied.u, .v = applied.v, .poles = poles, .weights = weights, .data_grid = applied.data_grid},
            sampling_, current().framing));
        return {};
    }
    catch (const std::exception &error)
    {
        return std::unexpected(error.what());
    }
}

auto EditableNurbsSurface::preview_pole_move(const std::size_t index, const formlight_geometry::geometry::Vec3 position)
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
            preview_ = sample_surface(std::move(definition), sampling_, current().framing);
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
