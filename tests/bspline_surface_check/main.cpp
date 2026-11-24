#include <cstdio>
#include <cstdlib>

import std;

void check_surface_evaluation();
void check_nurbs_pole_edits();
void check_nurbs_pole_drag();
void check_nurbs_presets();
void check_control_net_creation();
void check_point_grid_interpolation();
void check_point_grid_approximation();
void check_custom_surface_knots();
void check_surface_knot_insertion();
void check_surface_degree_elevation();
void check_nurbs_surface_projects();
void check_nurbs_resampling();
void check_nurbs_reference_grid();
void check_nurbs_mesh_drawing();
void check_nurbs_gpu_input();
void check_nurbs_gpu_normals();

int main()
{
    try
    {
        check_surface_evaluation();
        check_nurbs_pole_edits();
        check_nurbs_pole_drag();
        check_nurbs_presets();
        check_control_net_creation();
        check_point_grid_interpolation();
        check_point_grid_approximation();
        check_custom_surface_knots();
        check_surface_knot_insertion();
        check_surface_degree_elevation();
        check_nurbs_surface_projects();
        check_nurbs_resampling();
        check_nurbs_reference_grid();
        check_nurbs_mesh_drawing();
        check_nurbs_gpu_input();
        check_nurbs_gpu_normals();
        std::println("B-spline surface: 4x3 control net, parameter bounds, positions, derivatives, crease "
                     "and rational weights verified; preview isolines, pole edit history and pole dragging verified; "
                     "saddle preset and custom plane creation verified; control net and point-grid creation verified; "
                     "independent U/V degrees verified; "
                     "custom knots verified; NURBS v2 project round trips verified; "
                     "NURBS resampling verified; NURBS probe verified; Bezier surface presets verified; "
                     "NURBS reference grid verified; NURBS GPU input verified; NURBS GPU normal comparison verified; "
                     "NURBS mesh topology and framing verified; NURBS Bernstein probe verified");
        return EXIT_SUCCESS;
    }
    catch (const std::exception &error)
    {
        std::println(stderr, "bspline_surface_check: {}", error.what());
        return EXIT_FAILURE;
    }
}
