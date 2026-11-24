export module occt_lab.geometry.surfaces.bezier_patch;

import std;
export import occt_lab.geometry.surfaces.surface_isolines;

namespace occt_lab::geometry
{
export struct SurfaceSample
{
    Vec3 position;
    Vec3 normal;
};

export enum class PatchError
{
    WrongControlPointCount,
    NonFiniteControlPoint,
    NonFiniteParameter,
    ParameterOutOfRange,
    DegenerateNormal,
    InvalidSampling,
    NonFiniteEvaluation,
    NotPolynomialBicubic,
};

export class CubicBezierPatch
{
  public:
    static constexpr std::size_t side_length = 4;
    static constexpr double normal_tolerance = 1e-12;
    using ControlNet = std::array<Vec3, side_length * side_length>;

    // The control points are stored row-major, with u along columns and v along rows.
    [[nodiscard]] static std::expected<CubicBezierPatch, PatchError> create(std::span<const Vec3> control_points);
    [[nodiscard]] std::expected<SurfaceSample, PatchError> evaluate(double u, double v) const;
    [[nodiscard]] auto control_net() const -> const ControlNet &
    {
        return control_points_;
    }

  private:
    explicit CubicBezierPatch(const ControlNet &control_points);
    ControlNet control_points_;
};

namespace
{
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
} // namespace

std::expected<CubicBezierPatch, PatchError> CubicBezierPatch::create(const std::span<const Vec3> control_points)
{
    if (control_points.size() != side_length * side_length)
    {
        return std::unexpected(PatchError::WrongControlPointCount);
    }

    const bool has_invalid_point =
        std::ranges::any_of(control_points, [](const Vec3 &point)
                            { return !std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z); });
    if (has_invalid_point)
    {
        return std::unexpected(PatchError::NonFiniteControlPoint);
    }

    ControlNet copied_points{};
    std::ranges::copy(control_points, copied_points.begin());
    return CubicBezierPatch{copied_points};
}

std::expected<SurfaceSample, PatchError> CubicBezierPatch::evaluate(const double u, const double v) const
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
            const Vec3 &control_point = control_points_[row * side_length + column];
            position = position + control_point * (value_u[column] * value_v[row]);
            tangent_u = tangent_u + control_point * (derivative_u[column] * value_v[row]);
            tangent_v = tangent_v + control_point * (value_u[column] * derivative_v[row]);
        }
    }

    const Vec3 unnormalized_normal = cross(tangent_u, tangent_v);
    const double normal_length = std::hypot(unnormalized_normal.x, unnormalized_normal.y, unnormalized_normal.z);
    if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z) ||
        !std::isfinite(normal_length))
    {
        return std::unexpected(PatchError::NonFiniteEvaluation);
    }
    if (normal_length <= normal_tolerance)
    {
        return std::unexpected(PatchError::DegenerateNormal);
    }

    return SurfaceSample{
        .position = position,
        .normal = unnormalized_normal * (1.0 / normal_length),
    };
}

CubicBezierPatch::CubicBezierPatch(const ControlNet &control_points) : control_points_(control_points)
{
}

export [[nodiscard]] auto sample_patch_isolines(const CubicBezierPatch &patch, const PatchSampling sampling = {})
    -> std::expected<PatchIsolines, PatchError>
{
    const auto position = [&](double u, double v)
    { return patch.evaluate(u, v).transform([](const SurfaceSample &sample) { return sample.position; }); };
    return sample_surface_isolines({0, 1, 0, 1}, sampling, PatchError::InvalidSampling, position);
}
} // namespace occt_lab::geometry
