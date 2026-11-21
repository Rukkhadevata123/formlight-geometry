#include "../support/project_test_directory.hpp"
#include "document/nurbs_surface_project.hpp"
#include "geometry/surfaces/bezier_patch.hpp"

#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <utility>

namespace
{
using namespace occt_lab::geometry;
using namespace occt_lab::document;

constexpr std::string_view legacy_file = R"(occt-lab-bezier-patch 1
units mm
isolines 5 7 17
poles 4 4
-15 -9 0
-5 -9 -1
5 -9 -2
15 -9 -3
-15 -3 3
-5 -3 2
5 -3 11.625
15 -3 0
-15 3 6
-5 3 5
5 3 4
15 3 3
-15 9 9
-5 9 8
5 9 7
15 9 6
end
)";

void check_imported_geometry(const EditableNurbsSurface &model)
{
    const auto &definition = model.current().definition;
    const BSplineSurfaceAxis cubic{.degree = 3, .pole_count = 4, .knots = {{0, 4}, {1, 4}}};
    if (definition.u != cubic || definition.v != cubic || definition.weights != std::vector<double>(16, 1.0))
    {
        throw std::runtime_error("Bezier import did not produce two clamped cubic axes with unit weights");
    }
    CubicBezierPatch::ControlNet net;
    for (int v = 0; v < 4; ++v)
    {
        for (int u = 0; u < 4; ++u)
        {
            net[v * 4 + u] = {10.0 * u - 15, 6.0 * v - 9, 3.0 * v - u};
        }
    }
    net[6].z = 11.625;
    if (!std::ranges::equal(net, definition.poles))
    {
        throw std::runtime_error("Bezier control points changed order or position");
    }
    const auto bernstein = CubicBezierPatch::create(net).value();
    const BSplineSurface nurbs{definition};
    for (const auto [u, v] : {std::pair{0.0, 0.0}, std::pair{0.23, 0.71}, std::pair{1.0, 1.0}})
    {
        const auto expected = bernstein.evaluate(u, v).value();
        const auto [point, du, dv] = nurbs.evaluate_d1(u, v).value();
        const auto normal = gp_Vec{du.x, du.y, du.z}.Crossed(gp_Vec{dv.x, dv.y, dv.z}).Normalized();
        const auto &p = expected.position;
        const auto &n = expected.normal;
        if (!(std::hypot(point.x - p.x, point.y - p.y, point.z - p.z) <= 1e-10 &&
              std::hypot(normal.X() - n.x, normal.Y() - n.y, normal.Z() - n.z) <= 1e-12))
        {
            throw std::runtime_error("Imported surface differs from the legacy Bernstein position or oriented normal");
        }
    }
}

void check_sampling(const EditableNurbsSurface &model)
{
    const auto &lines = model.displayed().isolines;
    if (model.sampling() != PatchSampling{5, 7, 17} || lines.constant_u.size() != 5 || lines.constant_v.size() != 7 ||
        lines.constant_u.front().size() != 17 || lines.constant_v.back().size() != 17)
    {
        throw std::runtime_error("Imported isoline settings were lost");
    }
}
} // namespace

void check_bezier_project_import()
{
    const ProjectTestDirectory directory;
    const auto source = directory.path / "legacy.occtlab";
    std::ofstream(source, std::ios::binary) << legacy_file;
    auto model = open_nurbs_surface_project(source).value();
    check_imported_geometry(model);
    check_sampling(model);
    if (model.can_undo() || model.can_redo())
    {
        throw std::runtime_error("Import should start with an empty edit history");
    }
    model.preview_pole_move(6, {7, -3, 20}).value();
    check_sampling(model);
    model.commit_preview();
    model.undo();
    check_imported_geometry(model);
    model.redo();
    auto weights = model.current().definition.weights;
    weights[6] = 2.5;
    model.apply(model.current().definition.poles, weights).value();
    check_sampling(model);

    const auto converted = directory.path / "converted.occtlab";
    save_nurbs_surface_project(converted, model).value();
    const auto reopened = open_nurbs_surface_project(converted).value();
    check_sampling(reopened);
    if (reopened.current().definition != model.current().definition ||
        reopened.current().isolines.constant_u != model.current().isolines.constant_u ||
        reopened.current().isolines.constant_v != model.current().isolines.constant_v)
    {
        throw std::runtime_error("Saving and reopening lost the edited Bezier import");
    }
    std::ifstream original(source, std::ios::binary);
    if (std::string{std::istreambuf_iterator<char>{original}, {}} != legacy_file)
    {
        throw std::runtime_error("Opening the legacy file or saving a copy changed the source");
    }
    const auto bad = directory.path / "invalid.occtlab";
    for (const auto [field, replacement] : {std::pair{"patch 1", "patch 9"}, std::pair{"poles 4 4", "poles 3 4"},
                                            std::pair{"5 7 17", "5 7 1"}, std::pair{"end\n", ""}})
    {
        std::string text{legacy_file};
        text.replace(text.find(field), std::string_view{field}.size(), replacement);
        std::ofstream(bad, std::ios::binary) << text;
        if (open_nurbs_surface_project(bad))
        {
            throw std::runtime_error(std::format("Accepted malformed legacy patch field '{}'", field));
        }
    }
}
