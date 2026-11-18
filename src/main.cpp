#include "vulkan_window.hpp"

#include <SDL3/SDL_main.h>
#include <SDL3/SDL_version.h>
#include <Standard_Version.hxx>
#include <imgui.h>
#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <expected>
#include <print>
#include <ranges>
#include <span>
#include <string_view>
#include <utility>

namespace
{
struct Vec3
{
    double x;
    double y;
    double z;
};

struct SurfaceSample
{
    Vec3 position;
    Vec3 normal;
};

enum class PatchError
{
    WrongControlPointCount,
    NonFiniteControlPoint,
    NonFiniteParameter,
    ParameterOutOfRange,
    DegenerateNormal
};

constexpr Vec3 operator+(const Vec3 &left, const Vec3 &right)
{
    return {.x = left.x + right.x, .y = left.y + right.y, .z = left.z + right.z};
}

constexpr Vec3 operator*(const Vec3 &value, const double scale)
{
    return {.x = value.x * scale, .y = value.y * scale, .z = value.z * scale};
}

constexpr Vec3 cross(const Vec3 &left, const Vec3 &right)
{
    return {
        .x = left.y * right.z - left.z * right.y,
        .y = left.z * right.x - left.x * right.z,
        .z = left.x * right.y - left.y * right.x,
    };
}

double length(const Vec3 &value)
{
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

struct CubicBasis
{
    std::array<double, 4> value;
    std::array<double, 4> derivative;
};

CubicBasis cubic_basis(const double parameter)
{
    const double inverse = 1.0 - parameter;

    return {
        .value =
        {
            inverse * inverse * inverse,
            3.0 * parameter * inverse * inverse,
            3.0 * parameter * parameter * inverse,
            parameter * parameter * parameter,
        },
        .derivative =
        {
            -3.0 * inverse * inverse,
            3.0 * inverse * inverse - 6.0 * parameter * inverse,
            6.0 * parameter * inverse - 3.0 * parameter * parameter,
            3.0 * parameter * parameter,
        },
    };
}

class CubicBezierPatch
{
public:
    static std::expected<CubicBezierPatch, PatchError> create(std::span<const Vec3> control_points)
    {
        if (control_points.size() != control_point_count)
        {
            return std::unexpected(PatchError::WrongControlPointCount);
        }

        const bool has_invalid_point = std::ranges::any_of(
            control_points,
            [](const Vec3 &point)
            {
                return !std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z);
            });
        if (has_invalid_point)
        {
            return std::unexpected(PatchError::NonFiniteControlPoint);
        }

        std::array<Vec3, control_point_count> copied_points{};
        std::ranges::copy(control_points, copied_points.begin());
        return CubicBezierPatch{copied_points};
    }

    std::expected<SurfaceSample, PatchError> evaluate(const double u, const double v) const
    {
        if (!std::isfinite(u) || !std::isfinite(v))
        {
            return std::unexpected(PatchError::NonFiniteParameter);
        }
        if (u < 0.0 || u > 1.0 || v < 0.0 || v > 1.0)
        {
            return std::unexpected(PatchError::ParameterOutOfRange);
        }

        const auto [value_u, derivative_u] = cubic_basis(u);
        const auto [value_v, derivative_v] = cubic_basis(v);
        Vec3 position{};
        Vec3 tangent_u{};
        Vec3 tangent_v{};

        for (std::size_t row = 0; row < side_length; ++row)
        {
            for (std::size_t column = 0; column < side_length; ++column)
            {
                const Vec3 control_point = control_points_[row * side_length + column];
                position = position + control_point * (value_u[column] * value_v[row]);
                tangent_u = tangent_u + control_point * (derivative_u[column] * value_v[row]);
                tangent_v = tangent_v + control_point * (value_u[column] * derivative_v[row]);
            }
        }

        const Vec3 unnormalized_normal = cross(tangent_u, tangent_v);
        const double normal_length = length(unnormalized_normal);
        if (normal_length <= 1e-12)
        {
            return std::unexpected(PatchError::DegenerateNormal);
        }

        return SurfaceSample{
            .position = position,
            .normal = unnormalized_normal * (1.0 / normal_length),
        };
    }

private:
    static constexpr std::size_t side_length = 4;
    static constexpr std::size_t control_point_count = side_length * side_length;

    explicit CubicBezierPatch(const std::array<Vec3, control_point_count> &control_points)
        : control_points_(control_points)
    {
    }

    std::array<Vec3, control_point_count> control_points_;
};

std::string_view describe(const PatchError error)
{
    switch (error)
    {
    case PatchError::WrongControlPointCount:
        return "a cubic patch requires exactly 16 control points";
    case PatchError::NonFiniteControlPoint:
        return "control points must contain finite coordinates";
    case PatchError::NonFiniteParameter:
        return "surface parameters must be finite";
    case PatchError::ParameterOutOfRange:
        return "surface parameters must be in [0, 1]";
    case PatchError::DegenerateNormal:
        return "surface tangents do not define a normal";
    }

    return "unknown patch error";
}

void print_bezier_patch_samples()
{
    std::array<Vec3, 16> control_points{};
    for (std::size_t row = 0; row < 4; ++row)
    {
        for (std::size_t column = 0; column < 4; ++column)
        {
            const double height = (row == 1 || row == 2) && (column == 1 || column == 2) ? 0.4 : 0.0;
            control_points[row * 4 + column] = {
                .x = static_cast<double>(column),
                .y = static_cast<double>(row),
                .z = height,
            };
        }
    }

    const auto patch = CubicBezierPatch::create(control_points);
    if (!patch)
    {
        std::println("Could not create Bezier patch: {}", describe(patch.error()));
        return;
    }

    constexpr std::array parameters{
        std::pair{0.0, 0.0},
        std::pair{0.5, 0.5},
        std::pair{1.0, 1.0},
    };
    for (const auto &[u, v] : parameters)
    {
        const auto sample = patch->evaluate(u, v);
        if (!sample)
        {
            std::println("Could not evaluate patch: {}", describe(sample.error()));
            continue;
        }

        std::println("P({:.1f}, {:.1f}) = ({:.3f}, {:.3f}, {:.3f}), normal = ({:.3f}, {:.3f}, {:.3f})", u, v,
                     sample->position.x, sample->position.y, sample->position.z, sample->normal.x, sample->normal.y,
                     sample->normal.z);
    }

    if (const auto invalid_sample = patch->evaluate(1.25, 0.5); !invalid_sample)
    {
        std::println("Expected invalid input: {}", describe(invalid_sample.error()));
    }
}
} // namespace

auto main(int, char **) -> int
{
    try
    {
        std::println("Library header versions😈");
        std::println("OCCT: {}", OCC_VERSION_COMPLETE);
        std::println("SDL3: {}.{}.{}", SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_MICRO_VERSION);
        std::println("Dear ImGui: {}", IMGUI_VERSION);
        std::println("Vulkan: {}.{}.{}", VK_API_VERSION_MAJOR(VK_HEADER_VERSION_COMPLETE),
                     VK_API_VERSION_MINOR(VK_HEADER_VERSION_COMPLETE), VK_API_VERSION_PATCH(VK_HEADER_VERSION_COMPLETE));

        print_bezier_patch_samples();
        run_vulkan_window();
        return EXIT_SUCCESS;
    }
    catch (const std::exception &error)
    {
        std::println(stderr, "occt-lab: {}", error.what());
        return EXIT_FAILURE;
    }
}
