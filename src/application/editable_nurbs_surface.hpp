#pragma once

#include "geometry/surfaces/bspline_surface.hpp"

#include <optional>

class EditableNurbsSurface
{
  public:
    using Definition = occt_lab::geometry::BSplineSurfaceDefinition;

    struct Snapshot
    {
        Definition definition;
        occt_lab::geometry::PatchIsolines isolines;
    };

    explicit EditableNurbsSurface(Definition definition);
    [[nodiscard]] const Snapshot &current() const
    {
        return current_;
    }
    [[nodiscard]] bool can_undo() const
    {
        return history_.has_value() && !undone_;
    }
    [[nodiscard]] bool can_redo() const
    {
        return history_.has_value() && undone_;
    }

    [[nodiscard]] auto apply(const std::vector<occt_lab::geometry::Vec3> &poles, const std::vector<double> &weights)
        -> std::expected<void, std::string>;
    void undo();
    void redo();

  private:
    Snapshot current_;
    std::optional<Snapshot> history_;
    bool undone_ = false;
};
