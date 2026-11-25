export module formlight_geometry.geometry.surfaces.surface_isolines;

import std;
export import formlight_geometry.geometry.vec3;

export namespace formlight_geometry::geometry
{
// Map a parameter inside a finite increasing knot interval to [0, 1].
[[nodiscard]] inline double unit_parameter(const double value, const double low, const double high)
{
    const double span = high - low;
    return std::isfinite(span) ? (value - low) / span : (0.5 * value - 0.5 * low) / (0.5 * high - 0.5 * low);
}

struct SurfaceDomain
{
    double u_min, u_max;
    double v_min, v_max;
    [[nodiscard]] bool contains(double u, double v) const
    {
        return u >= u_min && u <= u_max && v >= v_min && v <= v_max;
    }
};

struct PatchSampling
{
    static constexpr int minimum_count = 2;
    static constexpr int max_lines = 33;
    static constexpr int max_samples = 129;

    int constant_u_lines = 9;
    int constant_v_lines = 9;
    int samples_per_line = 33;
    bool operator==(const PatchSampling &) const = default;
};

struct PatchIsolines
{
    using Line = std::vector<Vec3>;
    using Family = std::vector<Line>;
    Family constant_u;
    Family constant_v;
};

template <class Error, class Position>
[[nodiscard]] auto sample_surface_isolines(const SurfaceDomain domain, const PatchSampling sampling,
                                           const Error &invalid_sampling, Position position)
    -> std::expected<PatchIsolines, Error>
{
    const auto valid_count = [](const int count, const int maximum)
    { return count >= PatchSampling::minimum_count && count <= maximum; };
    if (!valid_count(sampling.constant_u_lines, PatchSampling::max_lines) ||
        !valid_count(sampling.constant_v_lines, PatchSampling::max_lines) ||
        !valid_count(sampling.samples_per_line, PatchSampling::max_samples))
    {
        return std::unexpected(invalid_sampling);
    }

    const auto sample_family = [&](PatchIsolines::Family &lines, const int count,
                                   const bool constant_u) -> std::expected<void, Error>
    {
        lines.reserve(count);
        for (int i = 0; i < count; ++i)
        {
            const double fraction = static_cast<double>(i) / (count - 1);
            auto &line = lines.emplace_back();
            line.reserve(sampling.samples_per_line);
            for (int j = 0; j < sampling.samples_per_line; ++j)
            {
                const double t = static_cast<double>(j) / (sampling.samples_per_line - 1);
                const double u = std::lerp(domain.u_min, domain.u_max, constant_u ? fraction : t);
                const double v = std::lerp(domain.v_min, domain.v_max, constant_u ? t : fraction);
                const auto sample = position(u, v);
                if (!sample)
                {
                    return std::unexpected(sample.error());
                }
                line.push_back(*sample);
            }
        }
        return {};
    };

    PatchIsolines lines;
    if (auto result = sample_family(lines.constant_u, sampling.constant_u_lines, true); !result)
    {
        return std::unexpected(result.error());
    }
    if (auto result = sample_family(lines.constant_v, sampling.constant_v_lines, false); !result)
    {
        return std::unexpected(result.error());
    }
    return lines;
}
} // namespace formlight_geometry::geometry
