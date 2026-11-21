#include "editable_patch.hpp"

#include "bezier_patch_presets.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace
{
using Patch = EditablePatch::Patch;

EditablePatch::Isolines checked_isolines(const Patch &patch, const EditablePatch::Sampling sampling)
{
    auto lines = sample_patch_isolines(patch, sampling);
    if (!lines)
    {
        if (lines.error() == occt_lab::geometry::PatchError::InvalidSampling)
        {
            throw std::runtime_error("Isoline counts or points per line are outside the supported range.");
        }
        if (lines.error() == occt_lab::geometry::PatchError::DegenerateNormal)
        {
            throw std::runtime_error("The edited patch has an undefined normal at an isoline sample.");
        }
        throw std::runtime_error("Could not sample the replacement Bezier patch");
    }
    return std::move(*lines);
}
} // namespace

EditablePatch::EditablePatch() : EditablePatch(occt_lab::raised_bezier_net(), Sampling{})
{
}

EditablePatch::EditablePatch(const Patch::ControlNet &poles, const Sampling sampling)
    : sampling_(sampling), current_(make_surface(poles, sampling))
{
}

bool EditablePatch::create_plane()
{
    const auto result = apply(occt_lab::planar_bezier_net());
    if (!result)
    {
        throw std::runtime_error(result.error());
    }
    return *result;
}

auto EditablePatch::make_surface(const Patch::ControlNet &poles, const Sampling sampling) -> std::unique_ptr<Surface>
{
    const auto candidate = Patch::create(poles);
    if (!candidate)
    {
        throw std::runtime_error("Control point coordinates must be finite.");
    }
    constexpr double gpu_limit = std::numeric_limits<float>::max();
    if (std::ranges::any_of(
            poles, [](const auto &p)
            { return std::abs(p.x) > gpu_limit || std::abs(p.y) > gpu_limit || std::abs(p.z) > gpu_limit; }))
    {
        throw std::runtime_error("Control point coordinates exceed the GPU float range.");
    }
    auto surface = std::make_unique<Surface>(*candidate);
    surface->isolines = checked_isolines(*candidate, sampling);
    return surface;
}

auto EditablePatch::prepare_preview(const Patch::ControlNet &poles) -> std::expected<bool, std::string>
{
    if (displayed().control_net() == poles)
    {
        return false;
    }
    if (current().control_net() == poles)
    {
        discard_preview();
        return true;
    }
    try
    {
        preview_ = make_surface(poles, sampling_);
        return true;
    }
    catch (const std::exception &error)
    {
        return std::unexpected(error.what());
    }
}

auto EditablePatch::apply(const Patch::ControlNet &poles) -> std::expected<bool, std::string>
{
    if (const auto result = prepare_preview(poles); !result)
    {
        return std::unexpected(result.error());
    }
    return commit_preview();
}

auto EditablePatch::preview_pole_move(const std::size_t index, const occt_lab::geometry::Vec3 &position)
    -> std::expected<bool, std::string>
{
    auto poles = current().control_net();
    if (index >= poles.size())
    {
        return std::unexpected("Control point index is outside the Bezier patch.");
    }
    poles[index] = position;
    return prepare_preview(poles);
}

bool EditablePatch::commit_preview()
{
    if (!preview_)
    {
        return false;
    }
    history_ = std::exchange(current_, std::move(preview_));
    undone_ = false;
    return true;
}

void EditablePatch::discard_preview()
{
    preview_.reset();
}

void EditablePatch::swap_history()
{
    history_->isolines = checked_isolines(history_->patch, sampling_);
    current_.swap(history_);
    discard_preview();
    undone_ = !undone_;
}

void EditablePatch::undo()
{
    if (can_undo())
    {
        swap_history();
    }
}

void EditablePatch::redo()
{
    if (can_redo())
    {
        swap_history();
    }
}

auto EditablePatch::resample(const Sampling sampling) -> std::expected<void, occt_lab::geometry::PatchError>
{
    auto lines = sample_patch_isolines(current(), sampling);
    if (!lines)
    {
        return std::unexpected(lines.error());
    }
    if (preview_)
    {
        auto preview_lines = sample_patch_isolines(preview_->patch, sampling);
        if (!preview_lines)
        {
            return std::unexpected(preview_lines.error());
        }
        preview_->isolines = std::move(*preview_lines);
    }
    current_->isolines = std::move(*lines);
    sampling_ = sampling;
    return {};
}
