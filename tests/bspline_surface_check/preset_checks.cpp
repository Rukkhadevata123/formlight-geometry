#include <Geom_BSplineSurface.hxx>

import std;
import formlight_geometry.application.bezier_patch_presets;
import formlight_geometry.application.editable_nurbs_surface;
import formlight_geometry.application.nurbs_surface_creation;
import formlight_geometry.geometry.surfaces.bernstein_reference;
import formlight_geometry.geometry.surfaces.bezier_patch;
import formlight_geometry.geometry.surfaces.bspline_surface;
import formlight_geometry.geometry.surfaces.occt_bezier_patch;

namespace
{
void check_bernstein_probe()
{
    using namespace formlight_geometry;
    using namespace geometry;
    auto definition = make_bezier_nurbs_surface(raised_bezier_net());
    definition.u.knots = {{-4, 4}, {8, 4}};
    definition.v.knots = {{2, 4}, {10, 4}};
    std::ranges::fill(definition.weights, 7.0);
    EditableNurbsSurface model{definition};
    for (const auto [u, v] : {std::pair{-4., 2.}, std::pair{-1., 8.}, std::pair{8., 10.}})
    {
        const auto bernstein = evaluate_bernstein_reference(definition, u, v).value();
        const auto probe = model.current().surface->probe(u, v).value();
        const auto diff = compare_surface_samples(bernstein, {probe.position, probe.normal.value()});
        if (!(diff.position_distance_mm < 1e-10 && diff.normal_angle_degrees < 1e-10))
        {
            throw std::runtime_error("Bernstein and OCCT disagree on the reparameterized bicubic patch");
        }
    }
    auto sample = [](const EditableNurbsSurface &editor)
    { return evaluate_bernstein_reference(editor.displayed().definition, -1, 8); };
    const auto original = sample(model).value();
    if (!(std::abs(original.position.z - 7.31689453125) <= 1e-10))
    {
        throw std::runtime_error("Bernstein probe used the wrong knot domain");
    }
    auto moved = definition.poles.at(5);
    moved.z += 10;
    model.preview_pole_move(5, moved).value();
    if (!(sample(model).value().position.z > original.position.z))
    {
        throw std::runtime_error("Bernstein probe missed the displayed pole preview");
    }
    model.discard_preview();
    auto weights = definition.weights;
    weights.back() = std::nextafter(weights.back(), 8.0);
    model.apply(definition.poles, weights).value();
    if (sample(model) != std::unexpected(PatchError::NotPolynomialBicubic))
    {
        throw std::runtime_error("Bernstein probe accepted unequal weights");
    }
    model.undo();
    if (sample(model).value().position != original.position)
    {
        throw std::runtime_error("Undo did not restore the polynomial Bernstein probe");
    }
    definition.weights.clear();
    if (!evaluate_bernstein_reference(definition, -1, 8) ||
        evaluate_bernstein_reference(definition, 9, 8) != std::unexpected(PatchError::ParameterOutOfRange))
    {
        throw std::runtime_error("Bernstein probe with implicit weights did not honor the knot bounds");
    }
    const auto multispan = make_planar_nurbs_surface({.u_poles = 5}).value();
    if (evaluate_bernstein_reference(multispan, 0.5, 0.5) != std::unexpected(PatchError::NotPolynomialBicubic) ||
        evaluate_bernstein_reference(make_saddle_surface(), 0.5, 0.5) !=
            std::unexpected(PatchError::NotPolynomialBicubic))
    {
        throw std::runtime_error("Bernstein probe accepted multiple spans or a non-cubic degree");
    }
    std::ranges::fill(definition.poles, Vec3{});
    if (evaluate_bernstein_reference(definition, -1, 8) != std::unexpected(PatchError::DegenerateNormal))
    {
        throw std::runtime_error("Bernstein probe accepted a collapsed patch normal");
    }
}

void check_bezier_presets()
{
    using namespace formlight_geometry;
    const auto plane = make_bezier_nurbs_surface(planar_bezier_net());
    const auto raised = make_bezier_nurbs_surface(raised_bezier_net());
    EditableNurbsSurface model{plane};
    if (model.current().surface->position(0.5, 0.5).value().z != 0.0)
    {
        throw std::runtime_error("The Bezier plane preset should lie in XY");
    }
    model.replace(raised).value();
    const auto &surface = *model.current().surface;
    if (!(std::abs(surface.position(0.5, 0.5).value().z - 16.03125) <= 1e-10))
    {
        throw std::runtime_error("The raised Bezier preset has the wrong center height");
    }
    // An off-center point also distinguishes the two interior rows and columns.
    constexpr double u = 0.25, v = 0.75;
    const auto point = surface.position(u, v).value();
    if (!(std::hypot(point.x + 22.5, point.y - 18.0, point.z - 14985.0 / 2048.0) <= 1e-10))
    {
        throw std::runtime_error("The raised Bezier preset changed its control net ordering");
    }
    model.undo();
    if (model.current().definition != plane)
    {
        throw std::runtime_error("Undo did not restore the Bezier plane preset");
    }
}

void check_independent_degrees()
{
    using formlight_geometry::SurfaceCreationParameters;
    SurfaceCreationParameters parameters{.u_poles = 7, .v_poles = 7, .u_degree = 2, .v_degree = 5};
    const auto plane = formlight_geometry::make_planar_nurbs_surface(parameters).value();
    if (plane.u.degree != 2 || plane.v.degree != 5 || plane.u.knots.size() != 6 || plane.v.knots.size() != 3 ||
        plane.u.knots.front().multiplicity != 3 || plane.v.knots.back().multiplicity != 6)
    {
        throw std::runtime_error("The plane did not use the requested degree and knots for each axis");
    }

    parameters.v_degree.reset();
    const auto cubic_v = formlight_geometry::make_planar_nurbs_surface(parameters).value();
    if (cubic_v.u != plane.u || cubic_v.v.degree != 3 || cubic_v.poles != plane.poles)
    {
        throw std::runtime_error("Defaulting V changed the U axis or the control grid");
    }
    EditableNurbsSurface model{plane};
    model.replace(cubic_v).value();
    if (model.current().definition != cubic_v || !model.can_undo())
    {
        throw std::runtime_error("A change in V degree alone should replace the surface");
    }
    model.undo();
    if (model.current().definition != plane)
    {
        throw std::runtime_error("Undo should restore both surface degrees and knot vectors");
    }

    const int occt_limit = Geom_BSplineSurface::MaxDegree();
    parameters = {.u_poles = SurfaceCreationParameters::max_poles, .v_poles = 2, .u_degree = occt_limit, .v_degree = 1};
    const auto highest = formlight_geometry::make_planar_nurbs_surface(parameters).value();
    const formlight_geometry::geometry::BSplineSurface reference{highest};
    if (highest.u.degree != occt_limit || !reference.position(0.25, 0.75))
    {
        throw std::runtime_error("The supported OCCT degree limit should produce an evaluable plane");
    }
    for (const auto &invalid :
         {SurfaceCreationParameters{.u_degree = 0}, SurfaceCreationParameters{.v_degree = -1},
          SurfaceCreationParameters{.u_poles = 4, .u_degree = 4}, SurfaceCreationParameters{.v_poles = 2, .v_degree = 2},
          SurfaceCreationParameters{.v_poles = SurfaceCreationParameters::max_poles, .v_degree = occt_limit + 1}})
    {
        if (formlight_geometry::make_planar_nurbs_surface(invalid))
        {
            throw std::runtime_error("New surface accepted an invalid degree");
        }
    }
}

void check_planar_creation()
{
    using formlight_geometry::SurfaceCreationParameters;
    for (const auto [u_count, v_count] :
         {std::pair{2, 2}, std::pair{6, 3}, std::pair{3, SurfaceCreationParameters::max_poles}})
    {
        const auto plane = formlight_geometry::make_planar_nurbs_surface({u_count, v_count, 120, 40}).value();
        if (plane.u.pole_count != u_count || plane.v.pole_count != v_count ||
            plane.u.degree != std::min(3, u_count - 1) || plane.v.degree != std::min(3, v_count - 1))
        {
            throw std::runtime_error("Planar surface axes do not match the requested grid");
        }
        for (int v = 0; v < v_count; ++v)
        {
            for (int u = 0; u < u_count; ++u)
            {
                const auto index = static_cast<std::size_t>(v * u_count + u);
                const auto &[x, y, z] = plane.poles.at(index);
                if (!(std::abs(x - (-60 + 120.0 * u / (u_count - 1))) <= 1e-10) ||
                    !(std::abs(y - (-20 + 40.0 * v / (v_count - 1))) <= 1e-10) || z != 0 || plane.weights.at(index) != 1)
                {
                    throw std::runtime_error("Planar control net lost its spacing, row order or unit weights");
                }
            }
        }
        const formlight_geometry::geometry::BSplineSurface surface{plane};
        const auto near = [](const auto &a, const formlight_geometry::geometry::Vec3 &b)
        { return std::hypot(a.x - b.x, a.y - b.y, a.z - b.z) <= 1e-10; };
        if (!near(surface.position(0.5, 0.5).value(), {}) ||
            !near(surface.position(0, 0).value(), plane.poles.front()) ||
            !near(surface.position(1, 1).value(), plane.poles.back()))
        {
            throw std::runtime_error("Created plane does not interpolate its corners or span the origin");
        }
    }

    for (const auto &invalid : {SurfaceCreationParameters{1, 4, 120, 40}, SurfaceCreationParameters{4, 65, 120, 40},
                                SurfaceCreationParameters{4, 4, 0, 40}, SurfaceCreationParameters{4, 4, 120, -1},
                                SurfaceCreationParameters{4, 4, std::numeric_limits<double>::quiet_NaN(), 40},
                                SurfaceCreationParameters{4, 4, 120, std::numeric_limits<double>::infinity()}})
    {
        if (formlight_geometry::make_planar_nurbs_surface(invalid))
        {
            throw std::runtime_error("New surface accepted invalid grid dimensions");
        }
    }
}
} // namespace

void check_nurbs_presets()
{
    check_bernstein_probe();
    check_bezier_presets();
    check_planar_creation();
    check_independent_degrees();
    const auto saddle = formlight_geometry::make_saddle_surface();
    const formlight_geometry::geometry::BSplineSurface surface{saddle};
    for (const double u : {0.0, 0.25, 0.5, 0.75, 1.0})
    {
        for (const double v : {0.0, 0.25, 0.5, 0.75, 1.0})
        {
            const double s = 2 * u - 1;
            const double t = 2 * v - 1;
            // The center pole is at the origin. Its extra unit of weight changes only the denominator.
            const double w = 1 + 4 * u * (1 - u) * v * (1 - v);
            const auto p = surface.position(u, v).value();
            if (!(std::hypot(p.x - 45 * s / w, p.y - 36 * t / w, p.z - 12 * (s * s - t * t) / w) <= 1e-10))
            {
                throw std::runtime_error("Saddle preset differs from its rational quadratic surface");
            }
        }
    }

    EditableNurbsSurface model{saddle};
    const auto saddle_frame = model.current().framing;
    auto poles = saddle.poles;
    poles.back().x += 10;
    model.apply(poles, saddle.weights).value();
    if (model.current().framing != saddle_frame)
    {
        throw std::runtime_error("Editing an outer saddle pole changed the framing");
    }
    const auto edited = model.current();
    const auto plane = formlight_geometry::make_planar_nurbs_surface({6, 3, 120, 40}).value();
    model.replace(plane).value();
    const auto plane_frame = model.current().framing;
    if (model.current().definition != plane || plane_frame == saddle_frame)
    {
        throw std::runtime_error("Creating a plane did not replace the geometry and framing");
    }
    model.replace(plane).value();
    model.undo();
    if (model.current().definition != edited.definition || model.current().framing != saddle_frame ||
        model.current().isolines.constant_v != edited.isolines.constant_v)
    {
        throw std::runtime_error("Undo should restore the edited saddle, its isolines and its original framing");
    }
    auto invalid = plane;
    invalid.poles.pop_back();
    if (model.replace(std::move(invalid)) || !model.can_redo() || model.current().definition != edited.definition)
    {
        throw std::runtime_error("An invalid replacement disturbed the edited saddle or its redo step");
    }
    model.redo();
    if (model.current().definition != plane || model.current().framing != plane_frame)
    {
        throw std::runtime_error("Redo should restore the created plane and its framing");
    }
}
