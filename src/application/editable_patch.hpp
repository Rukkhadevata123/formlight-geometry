#pragma once

#include "geometry/occt_bezier_patch.hpp"

#include <memory>

class EditablePatch
{
  public:
    using Patch = occt_lab::geometry::CubicBezierPatch;
    using Sampling = occt_lab::geometry::PatchSampling;
    using Isolines = occt_lab::geometry::PatchIsolines;

    EditablePatch();
    [[nodiscard]] const Patch &current() const
    {
        return current_->patch;
    }
    [[nodiscard]] const occt_lab::geometry::OcctBezierPatch &reference() const
    {
        return current_->reference;
    }
    [[nodiscard]] const Isolines &isolines() const
    {
        return isolines_;
    }
    [[nodiscard]] Sampling sampling() const
    {
        return sampling_;
    }
    [[nodiscard]] bool can_undo() const
    {
        return history_ && !undone_;
    }
    [[nodiscard]] bool can_redo() const
    {
        return history_ && undone_;
    }

    [[nodiscard]] bool create_plane();
    void undo();
    void redo();
    [[nodiscard]] auto resample(Sampling sampling) -> std::expected<void, occt_lab::geometry::PatchError>;

  private:
    struct Surface
    {
        Patch patch;
        occt_lab::geometry::OcctBezierPatch reference{patch};
    };

    // A successful swap leaves the previous surface in replacement for undo/redo.
    void activate(std::unique_ptr<Surface> &replacement);
    std::unique_ptr<Surface> current_;
    std::unique_ptr<Surface> history_;
    bool undone_ = false;
    Sampling sampling_;
    Isolines isolines_;
};
