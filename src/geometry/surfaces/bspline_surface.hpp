#pragma once

#include "geometry/bspline_knot.hpp"
#include "geometry/vec3.hpp"

#include <Geom_BSplineSurface.hxx>

#include <expected>
#include <string>
#include <vector>

namespace occt_lab::geometry
{
struct BSplineSurfaceAxis
{
    int degree;
    int pole_count;
    std::vector<BSplineKnot> knots;
};

struct BSplineSurfaceDefinition
{
    BSplineSurfaceAxis u;
    BSplineSurfaceAxis v;
    std::vector<Vec3> poles;     // v rows, with u varying fastest.
    std::vector<double> weights; // Same order as poles. An empty array assigns unit weights.
};

struct SurfaceDomain
{
    double u_min, u_max;
    double v_min, v_max;
    [[nodiscard]] bool contains(double u, double v) const;
};

struct SurfaceD1
{
    Vec3 position;
    Vec3 du;
    Vec3 dv;
};

// A non-periodic, clamped B-spline or NURBS surface.
class BSplineSurface
{
  public:
    explicit BSplineSurface(const BSplineSurfaceDefinition &definition);
    [[nodiscard]] SurfaceDomain domain() const;
    [[nodiscard]] auto position(double u, double v) const -> std::expected<Vec3, std::string>;
    // Endpoints use the interior derivative. An interior knot with only C0 continuity is rejected.
    [[nodiscard]] auto evaluate_d1(double u, double v) const -> std::expected<SurfaceD1, std::string>;

  private:
    Geom_BSplineSurface surface_;
};
} // namespace occt_lab::geometry
