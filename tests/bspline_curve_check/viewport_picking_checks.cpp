import std;
import occt_lab.geometry.vec3;
import occt_lab.render.viewport.clip_transform;

namespace
{
using occt_lab::geometry::Vec3;

// A translated, non-square orthographic view. The depth interval is [0, 1].
constexpr ClipTransform ortho{{2, 0, 0, 1}, {0, 4, 0, -1}, {0, 0, 1, 0.5F}, {0, 0, 0, 1}};
static_assert(ortho.intersect_z_plane(0.5, 0.25, 0) == Vec3{-0.25, 0.3125, 0});
static_assert(!ortho.intersect_z_plane(0, 0, -1));
static_assert(!ortho.intersect_z_plane(0, 0, 1));

// At (2, 1, 0) clip.w is 4, so the perspective division gives (0.75, 0.25).
constexpr ClipTransform perspective{{1, 0, 0, 1}, {0, 2, 0, -1}, {0, 0, 1, 2}, {0.5F, 0, 1, 3}};
static_assert(perspective.intersect_z_plane(0.75, 0.25, 0) == Vec3{2, 1, 0});
static_assert(!perspective.intersect_z_plane(0, 0, -5));

// Both screen axes mix X and Y, and the caller's frame puts world XY at local Z = -2.
constexpr ClipTransform oblique{{1, 1, 0, -1}, {-1, 1, 1, 2}, {0, 0, 1, 4}, {0, 1, -1, 4}};
static_assert(oblique.intersect_z_plane(0.25, 0.125, -2) == Vec3{1, 2, -2});

constexpr ClipTransform edge_on{{1, 0, 0, 0}, {0, 0, 1, 0}, {0, 1, 0, 0.5F}, {0, 0, 0, 1}};
static_assert(!edge_on.intersect_z_plane(0, 0, 0));
} // namespace
