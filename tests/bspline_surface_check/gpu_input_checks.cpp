#include "analytic_surface.hpp"
#include "render/compute/nurbs_grid_input.hpp"
#include "render/compute/nurbs_grid_report.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <span>
#include <stdexcept>

void check_nurbs_gpu_input()
{
    using occt_lab::render::pack_nurbs_grid;
    auto definition = occt_lab::test::make_analytic_surface();
    constexpr occt_lab::geometry::PatchGrid grid{3, 8};
    // 36 two-float4 vertices, 12 pole float4s, 7+8 U words and 5+18 V words.
    constexpr std::uint32_t bytes = 374 * sizeof(float);
    const auto input = pack_nurbs_grid(definition, grid, bytes);
    if (input.parameters != std::array{3U, 8U, 4U, 3U, 2U, 1U} || input.words.size() != 374)
    {
        throw std::runtime_error("Incorrect NURBS push constants or buffer size");
    }
    const std::span words{input.words};
    if (!std::ranges::all_of(words.first(288), [](float w) { return w == 0; }) ||
        !std::ranges::equal(words.subspan(288 + 4 * 6, 4), std::array{2.F, 2.5F, 17.F, 1.F}) || words[291] != 1.F / 7.F)
    {
        throw std::runtime_error("Incorrect output prefix, pole order or weight normalization");
    }
    const std::array u_knots{0.F, 0.F, 0.F, 1.F / 3.F, 1.F, 1.F, 1.F};
    const std::array v_knots{0.F, 0.F, 0.625F, 1.F, 1.F};
    if (!std::ranges::equal(words.subspan(336, 7), u_knots) || !std::ranges::equal(words.subspan(351, 5), v_knots))
    {
        throw std::runtime_error("Incorrect expanded GPU knots");
    }
    const auto check_axis = [](std::span<const float> pairs, std::span<const unsigned> expected)
    {
        for (std::size_t i = 0; i < expected.size(); ++i)
        {
            if (pairs[2 * i] != static_cast<float>(i) / static_cast<float>(expected.size() - 1) ||
                std::bit_cast<unsigned>(pairs[2 * i + 1]) != expected[i])
            {
                throw std::runtime_error("Incorrect GPU grid parameter, span or crease flag");
            }
        }
    };
    check_axis(words.subspan(343, 8), std::array{4U, 6U, 6U, 6U});
    check_axis(words.subspan(356), std::array{2U, 2U, 2U, 2U, 2U, 5U, 4U, 4U, 4U});
    auto nearby = definition;
    nearby.v.knots[1].value = std::nextafter(0.5, 1.0);
    const auto rounded = pack_nurbs_grid(nearby, grid, bytes);
    if (rounded.words[353] != rounded.words[366] || std::bit_cast<unsigned>(rounded.words[367]) != 2U)
    {
        throw std::runtime_error("Float rounding moved a GPU sample across a C0 crease");
    }
    for (auto &pole : definition.poles)
    {
        pole.x += 1e12;
    }
    const auto shifted = pack_nurbs_grid(definition, grid, bytes);
    if (shifted.origin.x != input.origin.x + 1e12 || shifted.words != input.words)
    {
        throw std::runtime_error("Translation changed the local GPU control net");
    }
    try
    {
        (void)pack_nurbs_grid(definition, grid, bytes - 1);
        throw std::runtime_error("Oversized NURBS storage descriptor accepted");
    }
    catch (const std::length_error &)
    {
    }
    definition.u.knots[1].value = 5.0 - 1e-8;
    try
    {
        (void)pack_nurbs_grid(definition, grid, bytes);
        throw std::runtime_error("NURBS GPU input silently merged distinct knots");
    }
    catch (const std::domain_error &)
    {
    }
}

void check_nurbs_gpu_normals()
{
    using namespace occt_lab;
    const geometry::SurfaceProbe smooth{{0, 0, 0}, geometry::Vec3{0, 0, 1}};
    const geometry::SurfaceProbe crease{{0, 0, 0}, std::unexpected("C0")};
    const auto report = [](std::array<float, 4> normal, const geometry::SurfaceProbe &sample, float x = 0)
    {
        const std::array gpu{render::NurbsGridVertex{{x, 0, 0, 1}, normal}};
        return render::compare_nurbs_grid(gpu, std::span{&sample, 1}, {}, 1e-4);
    };
    if (!report({0, 0, 1, 1}, smooth).starts_with("Passed") || !report({}, crease).starts_with("Passed"))
    {
        throw std::runtime_error("GPU normal comparison rejected matching samples");
    }
    if (!report({0, 0, -1, 1}, smooth).starts_with("Failed") || !report({}, smooth).starts_with("Failed") ||
        !report({0, 0, 1, 1}, crease).starts_with("Failed") || !report({}, crease, 2).starts_with("Failed"))
    {
        throw std::runtime_error("GPU comparison missed an orientation, availability or position mismatch");
    }
    for (const auto invalid : {std::array{0.F, 0.F, 2.F, 1.F}, std::array{0.F, 0.F, 0.F, -1.F},
                               std::array{std::numeric_limits<float>::quiet_NaN(), 0.F, 0.F, 0.F}})
    {
        if (!report(invalid, crease).starts_with("Failed"))
        {
            throw std::runtime_error("GPU comparison accepted an invalid normal");
        }
    }
}
