module;

#include <BSplCLib.hxx>
#include <cstddef>

export module occt_lab.render.compute.nurbs_grid_input;

import std;
import occt_lab.geometry.surfaces.bspline_surface;

namespace occt_lab::render
{
export struct NurbsGridVertex
{
    std::array<float, 4> position; // Local XYZ and rational denominator.
    std::array<float, 4> normal;   // Unit XYZ; W is 1 for valid, 0 for unavailable, -1 for numerical failure.
};
static_assert(sizeof(NurbsGridVertex) == 8 * sizeof(float) && offsetof(NurbsGridVertex, normal) == 4 * sizeof(float));

// Shader binding 0: grid vertices, pole float4s, then each axis's expanded knots
// followed by (parameter, packed span and crease flag) pairs.
export struct NurbsGridInput
{
    std::array<std::uint32_t, 6> parameters; // U/V segments, U/V pole counts, U/V degrees.
    std::vector<float> words;
    geometry::Vec3 origin;
    double extent_mm;
};

namespace
{
static_assert(BSplCLib::MaxDegree() <= 25); // Matches the shader's basis array size.
static_assert(sizeof(float) == 4);

void append_axis(std::vector<float> &words, const geometry::BSplineSurfaceAxis &axis, const std::uint32_t segments)
{
    const double low = axis.knots.front().value;
    const double high = axis.knots.back().value;
    float previous = -1.0F;
    for (const auto &[value, multiplicity] : axis.knots)
    {
        const float knot = static_cast<float>(geometry::unit_parameter(value, low, high));
        if (knot - previous < std::numeric_limits<float>::min())
        {
            throw std::domain_error("Distinct surface knots are too close for GPU float precision.");
        }
        words.insert(words.end(), multiplicity, knot);
        previous = knot;
    }
    // Select the span in double: rounding a nearby parameter to a float knot must not cross a crease.
    std::size_t knot_index = 0;
    auto span = static_cast<std::uint32_t>(axis.degree);
    for (std::uint32_t i = 0; i <= segments; ++i)
    {
        const double x = std::lerp(low, high, static_cast<double>(i) / segments);
        while (knot_index + 2 < axis.knots.size() && x >= axis.knots[knot_index + 1].value)
        {
            ++knot_index;
            span += axis.knots[knot_index].multiplicity;
        }
        const auto &knot = axis.knots[knot_index];
        const bool crease = knot_index != 0 && x == knot.value && knot.multiplicity == axis.degree;
        words.push_back(static_cast<float>(geometry::unit_parameter(x, low, high)));
        words.push_back(std::bit_cast<float>((span << 1) | static_cast<std::uint32_t>(crease)));
    }
}
} // namespace

// Accepts a definition already validated by the editor.
export [[nodiscard]] NurbsGridInput pack_nurbs_grid(const geometry::BSplineSurfaceDefinition &definition,
                                                    const geometry::PatchGrid grid,
                                                    const std::uint32_t max_buffer_bytes)
{
    if (!grid.valid())
    {
        throw std::invalid_argument("Invalid GPU grid subdivisions.");
    }
    const auto &[u, v, poles, weights] = definition;
    const auto knot_words = std::uint64_t(u.pole_count) + v.pole_count + u.degree + v.degree + 2;
    const auto parameter_words = 2ULL * (grid.u_segments + grid.v_segments + 2);
    const auto output_words = grid.vertex_count() * (sizeof(NurbsGridVertex) / sizeof(float));
    const auto word_count = output_words + 4ULL * poles.size() + knot_words + parameter_words;
    if (word_count > max_buffer_bytes / sizeof(float))
    {
        throw std::length_error("NURBS comparison exceeds the device storage buffer range.");
    }
    NurbsGridInput input{{grid.u_segments, grid.v_segments, static_cast<std::uint32_t>(u.pole_count),
                          static_cast<std::uint32_t>(v.pole_count), static_cast<std::uint32_t>(u.degree),
                          static_cast<std::uint32_t>(v.degree)},
                         {},
                         poles.front(),
                         0.0};
    input.words.reserve(static_cast<std::size_t>(word_count));
    input.words.resize(output_words); // A zero position W detects vertices the shader never wrote.
    const double scale = weights.empty() ? 1.0 : std::ranges::max(weights);
    for (std::size_t i = 0; i < poles.size(); ++i)
    {
        const auto &[x, y, z] = poles[i];
        const geometry::Vec3 local{x - input.origin.x, y - input.origin.y, z - input.origin.z};
        input.extent_mm = std::max({input.extent_mm, std::abs(local.x), std::abs(local.y), std::abs(local.z)});
        if (input.extent_mm > std::numeric_limits<float>::max())
        {
            throw std::domain_error("A surface control point exceeds the GPU float range.");
        }
        const float w = weights.empty() ? 1.0F : static_cast<float>(weights[i] / scale);
        if (w < std::numeric_limits<float>::min())
        {
            throw std::domain_error("The surface weight ratio is too small for GPU float precision.");
        }
        input.words.insert(input.words.end(),
                           {static_cast<float>(local.x), static_cast<float>(local.y), static_cast<float>(local.z), w});
    }
    append_axis(input.words, u, grid.u_segments);
    append_axis(input.words, v, grid.v_segments);
    return input;
}
} // namespace occt_lab::render
