#include <cstdio>
#include <cstdlib>
#include <exception>
#include <print>

void check_curve_geometry();
void check_curve_creation();
void check_curve_editing();
void check_curve_fitting();
void check_curve_projects();

int main()
{
    try
    {
        check_curve_geometry();
        check_curve_creation();
        check_curve_editing();
        check_curve_fitting();
        check_curve_projects();
        std::println("B-spline geometry, custom curve creation, cubic Bezier preset, "
                     "XYZ interpolation, control polygon creation, editing, history and project round trips verified");
        return EXIT_SUCCESS;
    }
    catch (const std::exception &error)
    {
        std::println(stderr, "bspline_curve_check: {}", error.what());
        return EXIT_FAILURE;
    }
}
