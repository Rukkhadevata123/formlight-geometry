#include "curve_project.hpp"

#include "project_io.hpp"

#include <cmath>
#include <iomanip>
#include <stdexcept>
#include <utility>

namespace occt_lab::document
{
namespace
{
using namespace geometry;
namespace fs = std::filesystem;

using io::expect_token;
using io::read_number;
using io::read_position;

void check_settings(const CurveProjectSettings &settings, const std::size_t pole_count)
{
    if (settings.object_id < 1 || settings.name.empty() || settings.name.find('\0') != std::string::npos ||
        settings.pole_number < 1 || static_cast<std::size_t>(settings.pole_number) > pole_count ||
        !std::isfinite(settings.knot_parameter) || !std::isfinite(settings.fit_tolerance) ||
        settings.fit_tolerance <= 0)
    {
        throw std::runtime_error("Invalid curve name, object ID, selected pole or experiment settings");
    }
}

CurveDefinition read_definition(std::istream &in)
{
    expect_token(in, "degree");
    CurveDefinition definition;
    definition.degree = read_number<int>(in, "degree");
    expect_token(in, "poles");
    const int pole_count = read_number<int>(in, "pole count");
    if (pole_count < 2)
    {
        throw std::runtime_error("A curve requires at least two poles");
    }
    for (int i = 0; i < pole_count; ++i)
    {
        definition.poles.push_back(read_position(in));
        definition.weights.push_back(read_number<double>(in, "weight"));
    }
    expect_token(in, "knots");
    const int knot_count = read_number<int>(in, "knot count");
    if (knot_count < 2 || knot_count > pole_count)
    {
        throw std::runtime_error("Invalid distinct knot count");
    }
    for (int i = 0; i < knot_count; ++i)
    {
        definition.knots.push_back(
            {.value = read_number<double>(in, "knot"), .multiplicity = read_number<int>(in, "multiplicity")});
    }

    return definition;
}

std::optional<RefinementComparison> read_refinement(std::istream &in, const CurveDefinition &definition)
{
    expect_token(in, "refinement");
    std::string kind;
    in >> kind;
    if (kind == "none")
    {
        return std::nullopt;
    }
    if (kind != "previous")
    {
        throw std::runtime_error("Unknown refinement report");
    }

    const int degree = read_number<int>(in, "previous degree");
    const int count = read_number<int>(in, "previous pole count");
    const auto deviation = read_number<double>(in, "refinement deviation");
    if (degree < 1 || degree > definition.degree || count <= degree ||
        static_cast<std::size_t>(count) >= definition.poles.size() || deviation < 0.0)
    {
        throw std::runtime_error("Invalid refinement report");
    }
    return RefinementComparison{.previous_degree = degree,
                                .previous_pole_count = static_cast<std::size_t>(count),
                                .max_sample_deviation = deviation};
}

SampledCurve read_fit(std::istream &in, const CurveDefinition &definition)
{
    expect_token(in, "fit");
    std::string kind;
    in >> kind;
    if (kind == "none")
    {
        return sample_curve(definition);
    }
    std::optional<double> tolerance;
    if (kind == "approximation")
    {
        tolerance = read_number<double>(in, "applied tolerance");
        if (*tolerance <= 0.0)
        {
            throw std::runtime_error("Applied tolerance must be positive");
        }
    }
    else if (kind != "interpolation")
    {
        throw std::runtime_error("Unknown fitting mode");
    }
    std::vector<Vec3> points;
    const int count = read_number<int>(in, "data point count");
    if (count < 2)
    {
        throw std::runtime_error("Point fitting requires at least two data points");
    }
    for (int i = 0; i < count; ++i)
    {
        points.push_back(read_position(in));
    }
    // Refinement may have changed the saved definition since the original fit.
    const auto reference = interpolate_curve(points);
    auto curve = sample_curve(definition, reference.data_points);
    if (tolerance)
    {
        curve.approximation = compare_to_interpolant(reference, *tolerance);
    }
    return curve;
}

CurveProject read_project(std::istream &in)
{
    expect_token(in, "occt-lab-curve");
    if (read_number<int>(in, "format version") != 1)
    {
        throw std::runtime_error("Unsupported curve project version");
    }
    expect_token(in, "units");
    expect_token(in, "mm");
    expect_token(in, "object");
    CurveProjectSettings settings;
    settings.object_id = read_number<int>(in, "object ID");
    in >> std::quoted(settings.name);
    expect_token(in, "editor");
    settings.pole_number = read_number<int>(in, "selected pole");
    settings.knot_parameter = read_number<double>(in, "knot parameter");
    settings.fit_tolerance = read_number<double>(in, "target tolerance");
    expect_token(in, "extent");
    const auto extent = read_number<double>(in, "framing extent");
    if (extent < 0.0)
    {
        throw std::runtime_error("Negative framing extent");
    }

    const auto definition = read_definition(in);
    check_settings(settings, definition.poles.size());
    const auto refinement = read_refinement(in, definition);
    auto curve = read_fit(in, definition);
    io::expect_end(in);

    curve.framing_extent = extent;
    curve.refinement = refinement;
    return {.curve = std::move(curve), .settings = std::move(settings)};
}

void write_project(std::ostream &out, const SampledCurve &curve, const CurveProjectSettings &settings)
{
    const auto &definition = curve.definition;
    out << "occt-lab-curve 1\nunits mm\nobject " << settings.object_id << ' ' << std::quoted(settings.name)
        << "\neditor " << settings.pole_number << ' ' << settings.knot_parameter << ' ' << settings.fit_tolerance
        << "\nextent " << curve.framing_extent << "\ndegree " << definition.degree << "\npoles "
        << definition.poles.size() << '\n';
    for (std::size_t i = 0; i < definition.poles.size(); ++i)
    {
        const auto &[x, y, z] = definition.poles[i];
        out << x << ' ' << y << ' ' << z << ' ' << definition.weights.at(i) << '\n';
    }
    out << "knots " << definition.knots.size() << '\n';
    for (const auto &[value, multiplicity] : definition.knots)
    {
        out << value << ' ' << multiplicity << '\n';
    }
    if (curve.refinement)
    {
        const auto &[degree, count, deviation] = *curve.refinement;
        out << "refinement previous " << degree << ' ' << count << ' ' << deviation << '\n';
    }
    else
    {
        out << "refinement none\n";
    }
    if (curve.approximation)
    {
        out << "fit approximation " << curve.approximation->requested_tolerance << ' ' << curve.data_points.size()
            << '\n';
    }
    else if (!curve.data_points.empty())
    {
        out << "fit interpolation " << curve.data_points.size() << '\n';
    }
    else
    {
        out << "fit none\n";
    }
    for (const auto &point : curve.data_points)
    {
        const auto &[x, y, z] = point.position;
        out << x << ' ' << y << ' ' << z << '\n';
    }
    out << "end\n";
}
} // namespace

auto open_curve_project(const fs::path &path) -> std::expected<CurveProject, std::string>
{
    try
    {
        auto in = io::open_input(path);
        return read_project(in);
    }
    catch (const std::exception &error)
    {
        return std::unexpected(error.what());
    }
}

auto save_curve_project(const fs::path &path, const geometry::SampledCurve &curve, const CurveProjectSettings &settings)
    -> std::expected<void, std::string>
{
    const auto write = [&](std::ostream &out)
    {
        check_settings(settings, curve.definition.poles.size());
        write_project(out, curve, settings);
    };
    return io::write_atomically(path, write);
}
} // namespace occt_lab::document
