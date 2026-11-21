#pragma once

#include "geometry/surfaces/occt_bezier_patch.hpp"

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
    [[nodiscard]] const Patch &displayed() const
    {
        return displayed_surface().patch;
    }
    [[nodiscard]] const occt_lab::geometry::OcctBezierPatch &reference() const
    {
        return displayed_surface().reference;
    }
    [[nodiscard]] const Isolines &isolines() const
    {
        return displayed_surface().isolines;
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
    [[nodiscard]] auto apply(const Patch::ControlNet &poles) -> std::expected<bool, std::string>;
    [[nodiscard]] auto preview_pole_move(std::size_t index, const occt_lab::geometry::Vec3 &position)
        -> std::expected<bool, std::string>;
    bool commit_preview();
    void discard_preview();
    void undo();
    void redo();
    [[nodiscard]] auto resample(Sampling sampling) -> std::expected<void, occt_lab::geometry::PatchError>;

  private:
    struct Surface
    {
        Patch patch;
        occt_lab::geometry::OcctBezierPatch reference{patch};
        Isolines isolines;
    };

    [[nodiscard]] const Surface &displayed_surface() const
    {
        return preview_ ? *preview_ : *current_;
    }
    [[nodiscard]] auto prepare_preview(const Patch::ControlNet &poles) -> std::expected<bool, std::string>;
    void swap_history();
    Sampling sampling_;
    std::unique_ptr<Surface> current_;
    std::unique_ptr<Surface> preview_;
    std::unique_ptr<Surface> history_;
    bool undone_ = false;
};
