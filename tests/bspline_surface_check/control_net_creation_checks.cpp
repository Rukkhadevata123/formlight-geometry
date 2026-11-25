import std;
import formlight_geometry.application.editable_nurbs_surface;
import formlight_geometry.application.nurbs_surface_creation;
import formlight_geometry.document.nurbs_surface_project;
import formlight_geometry.geometry.bspline_knot;
import formlight_geometry.test.support.project_test_directory;

void check_control_net_creation()
{
    using namespace formlight_geometry;
    SurfacePoleRows net;
    resize_surface_net(net, 4, 3);
    net[1][2].position.z = 16;
    net[1][2].weight = 2;
    const auto original_net = net;
    resize_surface_net(net, 6, 4);
    resize_surface_net(net, 4, 3);
    if (net != original_net)
    {
        throw std::runtime_error("Resizing a control net moved an overlapping U/V cell");
    }

    const SurfaceCreationParameters parameters{.u_poles = 4, .v_poles = 3};
    const auto def = make_control_net_surface(parameters, net).value();
    if (def.u.degree != 3 || def.v.degree != 2 || def.poles.at(6) != net[1][2].position || def.weights.at(6) != 2)
    {
        throw std::runtime_error("Expected the weighted pole at U=3, V=2 of a 4-by-3 net");
    }
    EditableNurbsSurface model{make_saddle_surface()};
    const auto previous = model.current().surface;
    const auto frame = model.current().framing;
    model.preview_replacement(def, frame).value();
    // At (1/4,1/2) the unweighted basis product of (U=3,V=2) is (9/64)*(1/2).
    const auto z = model.displayed().surface->position(0.25, 0.5).value().z;
    if (!(std::abs(z - 288.0 / 137) < 1e-12) || model.displayed().framing != frame ||
        model.current().surface != previous || model.can_undo())
    {
        throw std::runtime_error("The control net preview changed history, framing or the rational sample");
    }
    model.discard_preview();
    if (model.displayed().surface != previous)
    {
        throw std::runtime_error("Cancel did not restore the original surface");
    }
    model.preview_replacement(def).value();
    const auto preview = model.displayed().surface;
    model.commit_preview();
    model.undo();
    if (model.current().surface != previous)
    {
        throw std::runtime_error("Creating a surface from a control net must be one history entry");
    }
    model.redo();
    if (model.current().surface != preview)
    {
        throw std::runtime_error("Redo must restore the preview that was accepted");
    }
    const ProjectTestDirectory directory;
    const auto path = directory.path / "control-net.occtlab";
    document::save_nurbs_surface_project(path, model).value();
    if (document::open_nurbs_surface_project(path).value().current().definition != def)
    {
        throw std::runtime_error("Reopening changed the non-square weighted control net");
    }
    auto custom = parameters;
    custom.u_degree = 2;
    custom.u_knots = std::vector<geometry::BSplineKnot>{{-2, 3}, {1, 1}, {5, 3}};
    const auto nonuniform = make_control_net_surface(custom, net).value();
    if (nonuniform.u.knots != *custom.u_knots || nonuniform.u.degree != 2 || nonuniform.v != def.v ||
        nonuniform.poles != def.poles || nonuniform.weights != def.weights)
    {
        throw std::runtime_error("Custom U knots must preserve the entered net and its V axis");
    }
    net[1][2].weight = 0;
    const auto invalid = make_control_net_surface(parameters, net);
    if (invalid || !invalid.error().contains("U 3, V 2"))
    {
        throw std::runtime_error("Invalid weight must identify its U/V cell");
    }
    net[1].pop_back();
    if (make_control_net_surface(parameters, net))
    {
        throw std::runtime_error("A ragged control net was accepted");
    }
}
