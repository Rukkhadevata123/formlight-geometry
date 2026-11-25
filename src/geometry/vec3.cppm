export module formlight_geometry.geometry.vec3;

export namespace formlight_geometry::geometry
{
struct Vec3
{
    double x;
    double y;
    double z;

    bool operator==(const Vec3 &) const = default;
};

[[nodiscard]] constexpr Vec3 operator+(const Vec3 &left, const Vec3 &right) noexcept
{
    return {.x = left.x + right.x, .y = left.y + right.y, .z = left.z + right.z};
}

[[nodiscard]] constexpr Vec3 operator*(const Vec3 &value, const double scale) noexcept
{
    return {.x = value.x * scale, .y = value.y * scale, .z = value.z * scale};
}

[[nodiscard]] constexpr Vec3 cross(const Vec3 &left, const Vec3 &right) noexcept
{
    return {
        .x = left.y * right.z - left.z * right.y,
        .y = left.z * right.x - left.x * right.z,
        .z = left.x * right.y - left.y * right.x,
    };
}
} // namespace formlight_geometry::geometry
