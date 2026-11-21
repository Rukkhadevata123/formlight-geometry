#include "nurbs_surface_project.hpp"

#include "project_io.hpp"

#include <utility>

namespace occt_lab::document
{
namespace
{
void write_axis(std::ostream &out, const char *name, const geometry::BSplineSurfaceAxis &axis)
{
    out << name << " degree " << axis.degree << " poles " << axis.pole_count << " knots " << axis.knots.size() << '\n';
    for (const auto &[value, multiplicity] : axis.knots)
    {
        out << value << ' ' << multiplicity << '\n';
    }
}

geometry::BSplineSurfaceAxis read_axis(std::istream &in, const char *name)
{
    io::expect_token(in, name);
    io::expect_token(in, "degree");
    const int degree = io::read_number<int>(in, "degree");
    io::expect_token(in, "poles");
    const int poles = io::read_number<int>(in, "pole count");
    io::expect_token(in, "knots");
    const int distinct = io::read_number<int>(in, "distinct knot count");
    if (degree < 1 || degree > Geom_BSplineSurface::MaxDegree() || poles <= degree || distinct < 2 || distinct > poles)
    {
        throw std::runtime_error(std::format("Invalid {} degree, pole count or knot count", name));
    }

    geometry::BSplineSurfaceAxis axis{.degree = degree, .pole_count = poles};
    for (int i = 0; i < distinct; ++i)
    {
        axis.knots.push_back({.value = io::read_number<double>(in, "knot value"),
                              .multiplicity = io::read_number<int>(in, "knot multiplicity")});
    }
    if (const auto valid = geometry::validate_clamped_knots(axis.knots, degree, poles); !valid)
    {
        throw std::runtime_error(std::format("{} knots: {}", name, valid.error()));
    }
    return axis;
}
} // namespace

auto save_nurbs_surface_project(const std::filesystem::path &path, const EditableNurbsSurface &model)
    -> std::expected<void, std::string>
{
    const auto write = [&](std::ostream &out)
    {
        const auto &definition = model.current().definition;
        out << "occt-lab-nurbs-surface 1\nunits mm\n";
        write_axis(out, "U", definition.u);
        write_axis(out, "V", definition.v);
        out << "control_net\n";
        for (std::size_t i = 0; i < definition.poles.size(); ++i)
        {
            const auto &[x, y, z] = definition.poles[i];
            out << x << ' ' << y << ' ' << z << ' ' << definition.weights.at(i) << '\n';
        }
        out << "end\n";
    };
    return io::write_atomically(path, write);
}

auto open_nurbs_surface_project(const std::filesystem::path &path) -> std::expected<EditableNurbsSurface, std::string>
{
    try
    {
        auto in = io::open_input(path);
        io::expect_token(in, "occt-lab-nurbs-surface");
        if (io::read_number<int>(in, "format version") != 1)
        {
            return std::unexpected("Unsupported NURBS surface project version");
        }
        io::expect_token(in, "units");
        io::expect_token(in, "mm");
        EditableNurbsSurface::Definition definition{.u = read_axis(in, "U"), .v = read_axis(in, "V")};
        const auto count = definition.control_net_size();
        io::expect_token(in, "control_net");
        for (std::size_t i = 0; i < count; ++i)
        {
            definition.poles.push_back(io::read_position(in));
            definition.weights.push_back(io::read_number<double>(in, "pole weight"));
        }
        io::expect_end(in);
        return EditableNurbsSurface{std::move(definition)};
    }
    catch (const std::exception &error)
    {
        return std::unexpected(error.what());
    }
}
} // namespace occt_lab::document
