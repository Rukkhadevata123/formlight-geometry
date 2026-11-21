#pragma once

#include "geometry/vec3.hpp"

#include <charconv>
#include <cmath>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace occt_lab::document::io
{
void expect_token(std::istream &in, std::string_view expected);
void expect_end(std::istream &in);

template <class T> T read_number(std::istream &in, const std::string_view field)
{
    std::string token;
    in >> token;
    const auto end = token.data() + token.size();
    T value{};
    const auto [next, error] = std::from_chars(token.data(), end, value);
    if (error != std::errc{} || next != end || !std::isfinite(value))
    {
        throw std::runtime_error(std::format("Invalid or missing {}", field));
    }
    return value;
}

geometry::Vec3 read_position(std::istream &in);
std::ifstream open_input(const std::filesystem::path &path);
[[nodiscard]] auto write_atomically(const std::filesystem::path &path, const std::function<void(std::ostream &)> &write)
    -> std::expected<void, std::string>;
} // namespace occt_lab::document::io
