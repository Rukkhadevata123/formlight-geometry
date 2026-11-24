module;

#include <Bnd_Box.hxx>

export module occt_lab.application.editable_nurbs_surface;

import std;
import occt_lab.application.single_step_history;
import occt_lab.geometry.surfaces.bspline_surface;

export class EditableNurbsSurface
{
  public:
    using Definition = occt_lab::geometry::BSplineSurfaceDefinition;

    struct Framing
    {
        occt_lab::geometry::Vec3 center;
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
        std::shared_ptr<const occt_lab::geometry::BSplineSurface> surface;
        occt_lab::geometry::PatchIsolines isolines;
        Framing framing;
        std::optional<double> max_data_point_error_mm;
    };

    explicit EditableNurbsSurface(Definition definition, occt_lab::geometry::PatchSampling sampling = {});
    [[nodiscard]] occt_lab::geometry::PatchSampling sampling() const
    {
        return sampling_;
    }
    [[nodiscard]] auto replace(Definition definition) -> std::expected<void, std::string>;
    [[nodiscard]] auto preview_replacement(Definition definition, std::optional<Framing> framing = {})
        -> std::expected<void, std::string>;
    [[nodiscard]] auto resample(occt_lab::geometry::PatchSampling sampling) -> std::expected<void, std::string>;
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

    [[nodiscard]] auto apply(const std::vector<occt_lab::geometry::Vec3> &poles, const std::vector<double> &weights)
        -> std::expected<void, std::string>;
    [[nodiscard]] auto preview_pole_move(std::size_t index, occt_lab::geometry::Vec3 position)
        -> std::expected<void, std::string>;
    void commit_preview();
    void discard_preview();
    void undo();
    void redo();

  private:
    void commit(Snapshot next);
    occt_lab::geometry::PatchSampling sampling_;
    SingleStepHistory<Snapshot> history_;
    std::optional<Snapshot> preview_;
};

namespace
{
EditableNurbsSurface::Framing frame_control_net(const std::vector<occt_lab::geometry::Vec3> &poles)
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

occt_lab::geometry::PatchIsolines sample_isolines(const occt_lab::geometry::BSplineSurface &surface,
                                                  const occt_lab::geometry::PatchSampling sampling)
{
    auto lines = surface.isolines(sampling);
    if (!lines)
    {
        throw std::runtime_error(lines.error());
    }
    return std::move(*lines);
}

std::optional<double> max_grid_deviation(const EditableNurbsSurface::Definition &definition,
                                         const occt_lab::geometry::BSplineSurface &surface)
{
    if (definition.interpolation_points.empty())
    {
        return std::nullopt;
    }
    if (definition.interpolation_points.size() != definition.poles.size())
    {
        throw std::invalid_argument("The interpolation grid dimensions do not match the surface.");
    }
    const auto bounds = surface.domain();
    const int columns = definition.u.pole_count, rows = definition.v.pole_count;
    double max_error = 0;
    for (int v = 0; v < rows; ++v)
    {
        const double pv = std::lerp(bounds.v_min, bounds.v_max, static_cast<double>(v) / (rows - 1));
        for (int u = 0; u < columns; ++u)
        {
            const double pu = std::lerp(bounds.u_min, bounds.u_max, static_cast<double>(u) / (columns - 1));
            const auto p = surface.position(pu, pv);
            if (!p)
            {
                throw std::runtime_error(p.error());
            }
            const auto &q = definition.interpolation_points[v * columns + u];
            const double distance = std::hypot(p->x - q.x, p->y - q.y, p->z - q.z);
            if (!std::isfinite(distance))
            {
                throw std::invalid_argument("The data-point deviation exceeds the finite coordinate range.");
            }
            max_error = std::max(max_error, distance);
        }
    }
    return max_error;
}

EditableNurbsSurface::Snapshot sample_surface(EditableNurbsSurface::Definition definition,
                                              const occt_lab::geometry::PatchSampling sampling,
                                              std::optional<EditableNurbsSurface::Framing> framing = {})
{
    if (definition.weights.empty())
    {
        definition.weights.assign(definition.poles.size(), 1.0);
    }
    auto surface = std::make_shared<const occt_lab::geometry::BSplineSurface>(definition);
    const auto max_error = max_grid_deviation(definition, *surface);
    auto lines = sample_isolines(*surface, sampling);
    if (!framing)
    {
        framing = frame_control_net(definition.poles);
    }
    return {.definition = std::move(definition), .surface = std::move(surface), .isolines = std::move(lines),
            .framing = *framing, .max_data_point_error_mm = max_error};
}
} // namespace

EditableNurbsSurface::EditableNurbsSurface(Definition definition, const occt_lab::geometry::PatchSampling sampling)
    : sampling_(sampling), history_(sample_surface(std::move(definition), sampling))
{
}

auto EditableNurbsSurface::replace(Definition definition) -> std::expected<void, std::string>
{
    return preview_replacement(std::move(definition)).transform([this] { commit_preview(); });
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

auto EditableNurbsSurface::resample(const occt_lab::geometry::PatchSampling sampling)
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
        occt_lab::geometry::PatchIsolines alternate_lines;
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
        commit(sample_surface({.u = applied.u, .v = applied.v, .poles = poles, .weights = weights,
                               .interpolation_points = applied.interpolation_points}, sampling_,
                              current().framing));
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
