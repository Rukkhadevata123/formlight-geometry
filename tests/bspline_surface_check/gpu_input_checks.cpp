#include "analytic_surface.hpp"
#include "render/compute/nurbs_position_input.hpp"

#include <algorithm>
#include <array>
#include <span>
#include <stdexcept>

void check_nurbs_gpu_input()
{
    using occt_lab::render::pack_nurbs_positions;
    auto definition = occt_lab::test::make_analytic_surface();
    constexpr occt_lab::geometry::PatchGrid grid{3, 8};
    // 36 output float4s, 12 pole float4s, 7+4 U words and 5+9 V words.
    constexpr std::uint32_t bytes = 217 * sizeof(float);
    const auto input = pack_nurbs_positions(definition, grid, bytes);
    if (input.parameters != std::array{3U, 8U, 4U, 3U, 2U, 1U} || input.words.size() != 217)
    {
        throw std::runtime_error("Incorrect NURBS push constants or buffer size");
    }
    const std::span words{input.words};
    if (!std::ranges::all_of(words.first(144), [](float w) { return w == 0; }) ||
        !std::ranges::equal(words.subspan(144 + 4 * 6, 4), std::array{2.F, 2.5F, 17.F, 1.F}) || words[147] != 1.F / 7.F)
    {
        throw std::runtime_error("Incorrect output prefix, pole order or weight normalization");
    }
    const std::array u{0.F, 0.F, 0.F, 1.F / 3.F, 1.F, 1.F, 1.F, 0.F, 1.F / 3.F, 2.F / 3.F, 1.F};
    const std::array v{0.F, 0.F, 0.625F, 1.F, 1.F, 0.F, 0.125F, 0.25F, 0.375F, 0.5F, 0.625F, 0.75F, 0.875F, 1.F};
    if (!std::ranges::equal(words.subspan(192, 11), u) || !std::ranges::equal(words.subspan(203), v))
    {
        throw std::runtime_error("Incorrect expanded knots or grid parameters");
    }
    for (auto &pole : definition.poles)
    {
        pole.x += 1e12;
    }
    const auto shifted = pack_nurbs_positions(definition, grid, bytes);
    if (shifted.origin.x != input.origin.x + 1e12 || shifted.words != input.words)
    {
        throw std::runtime_error("Translation changed the local GPU control net");
    }
    try
    {
        (void)pack_nurbs_positions(definition, grid, bytes - 1);
        throw std::runtime_error("Oversized NURBS storage descriptor accepted");
    }
    catch (const std::length_error &)
    {
    }
    definition.u.knots[1].value = 5.0 - 1e-8;
    try
    {
        (void)pack_nurbs_positions(definition, grid, bytes);
        throw std::runtime_error("NURBS GPU input silently merged distinct knots");
    }
    catch (const std::domain_error &)
    {
    }
}
