#pragma once

#include "geometry/surfaces/bspline_surface.hpp"
#include "single_step_history.hpp"

#include <optional>

class EditableNurbsSurface
{
  public:
    using Definition = occt_lab::geometry::BSplineSurfaceDefinition;

    struct Framing
    {
        occt_lab::geometry::Vec3 center;
        double span;
        bool operator==(const Framing &) const = default;
    };

    struct Snapshot
    {
        Definition definition;
        occt_lab::geometry::PatchIsolines isolines;
        Framing framing;
    };

    explicit EditableNurbsSurface(Definition definition, occt_lab::geometry::PatchSampling sampling = {});
    [[nodiscard]] occt_lab::geometry::PatchSampling sampling() const
    {
        return sampling_;
    }
    [[nodiscard]] auto replace(Definition definition) -> std::expected<void, std::string>;
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
