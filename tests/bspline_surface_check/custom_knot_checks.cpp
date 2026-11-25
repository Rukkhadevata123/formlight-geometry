import std;
import formlight_geometry.application.editable_nurbs_surface;
import formlight_geometry.application.nurbs_surface_creation;
import formlight_geometry.geometry.surfaces.bspline_surface;

void check_custom_surface_knots()
{
    using KnotList = std::vector<formlight_geometry::geometry::BSplineKnot>;
    const KnotList u{{-2, 3}, {0, 2}, {3, 1}, {5, 3}};
    const KnotList v{{-4, 2}, {-1, 1}, {1, 1}, {4, 2}};
    formlight_geometry::SurfaceCreationParameters parameters{.u_poles = 6, .v_poles = 4, .u_degree = 2, .v_degree = 1};
    const auto uniform = formlight_geometry::make_planar_nurbs_surface(parameters).value();
    parameters.u_knots = u;
    const auto custom_u = formlight_geometry::make_planar_nurbs_surface(parameters).value();
    if (custom_u.u.knots != u || custom_u.v != uniform.v)
    {
        throw std::runtime_error("Overriding U knots changed the automatic V axis");
    }
    parameters.v_knots = v;
    const auto plane = formlight_geometry::make_planar_nurbs_surface(parameters).value();
    if (plane.u.knots != u || plane.v.knots != v || plane.poles != uniform.poles)
    {
        throw std::runtime_error("Creating a plane changed its custom knot vectors or control net");
    }
    const formlight_geometry::geometry::BSplineSurface surface{plane};
    const auto [u_min, u_max, v_min, v_max] = surface.domain();
    if (u_min != -2 || u_max != 5 || v_min != -4 || v_max != 4)
    {
        throw std::runtime_error("The custom surface lost its parameter intervals");
    }
    const auto point = surface.position(0, -1).value();
    if (!(std::hypot(point.x + 9, point.y + 12, point.z) <= 1e-10))
    {
        throw std::runtime_error("Unexpected surface position at the repeated U knot");
    }
    EditableNurbsSurface model{uniform};
    model.replace(plane).value();
    model.undo();
    if (model.current().definition != uniform || !model.can_redo())
    {
        throw std::runtime_error("Undo did not restore the original knots");
    }
    for (const auto &invalid : {KnotList{}, KnotList{{-2, 3}, {0, 3}, {5, 3}},
                                KnotList{{-2, 2}, {0, 2}, {3, 1}, {5, 3}}, KnotList{{-2, 3}, {0, 1}, {5, 3}},
                                KnotList{{-2, 3}, {0, 2}, {0, 1}, {5, 3}}, KnotList{{-2, 3}, {3, 2}, {0, 1}, {5, 3}},
                                KnotList{{-2, 3}, {0, 2}, {std::numeric_limits<double>::quiet_NaN(), 1}, {5, 3}}})
    {
        parameters.u_knots = invalid;
        const auto result = formlight_geometry::make_planar_nurbs_surface(parameters);
        if (result || !result.error().starts_with("U knots:"))
        {
            throw std::runtime_error("Invalid custom U knots should produce a U-axis diagnostic");
        }
    }
    parameters.u_knots.reset();
    const auto custom_v = formlight_geometry::make_planar_nurbs_surface(parameters).value();
    if (custom_v.u != uniform.u || custom_v.v.knots != v)
    {
        throw std::runtime_error("Returning U to uniform knots changed the custom V axis");
    }
    parameters.v_knots->back().multiplicity = 0;
    const auto rejected = formlight_geometry::make_planar_nurbs_surface(parameters)
                              .and_then([&](auto definition) { return model.replace(std::move(definition)); });
    if (rejected || !rejected.error().starts_with("V knots:") || !model.can_redo() ||
        model.current().definition != uniform)
    {
        throw std::runtime_error("Invalid V knots should preserve the surface and redo history");
    }
    model.redo();
    if (model.current().definition != plane)
    {
        throw std::runtime_error("Redo did not restore the custom knots");
    }
}
