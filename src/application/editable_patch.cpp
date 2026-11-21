#include "editable_patch.hpp"

#include <stdexcept>
#include <utility>

namespace
{
using Patch = EditablePatch::Patch;

Patch::ControlNet planar_control_net()
{
    constexpr auto side = Patch::side_length;
    Patch::ControlNet net{};
    for (std::size_t row = 0; row < side; ++row)
    {
        for (std::size_t column = 0; column < side; ++column)
        {
            net[row * side + column] = {.x = -45.0 + 30.0 * static_cast<double>(column),
                                        .y = -36.0 + 24.0 * static_cast<double>(row),
                                        .z = 0.0};
        }
    }
    return net;
}

Patch raised_example()
{
    auto net = planar_control_net();
    constexpr auto side = Patch::side_length;
    net[side + 1].z = 40;
    net[side + 2].z = 28;
    net[2 * side + 1].z = 14;
    net[2 * side + 2].z = 32;
    return Patch::create(net).value();
}
} // namespace

EditablePatch::EditablePatch()
{
    auto initial = std::make_unique<Surface>(raised_example());
    activate(initial);
}

void EditablePatch::activate(std::unique_ptr<Surface> &replacement)
{
    auto lines = sample_patch_isolines(replacement->patch, sampling_);
    if (!lines)
    {
        throw std::runtime_error("Could not sample the replacement Bezier patch");
    }
    current_.swap(replacement);
    isolines_ = std::move(*lines);
}

bool EditablePatch::create_plane()
{
    const auto net = planar_control_net();
    if (current().control_net() == net)
    {
        return false;
    }
    auto replacement = std::make_unique<Surface>(Patch::create(net).value());
    activate(replacement);
    history_ = std::move(replacement);
    undone_ = false;
    return true;
}

void EditablePatch::undo()
{
    if (can_undo())
    {
        activate(history_);
        undone_ = true;
    }
}

void EditablePatch::redo()
{
    if (can_redo())
    {
        activate(history_);
        undone_ = false;
    }
}

auto EditablePatch::resample(const Sampling sampling) -> std::expected<void, occt_lab::geometry::PatchError>
{
    auto lines = sample_patch_isolines(current(), sampling);
    if (!lines)
    {
        return std::unexpected(lines.error());
    }
    isolines_ = std::move(*lines);
    sampling_ = sampling;
    return {};
}
