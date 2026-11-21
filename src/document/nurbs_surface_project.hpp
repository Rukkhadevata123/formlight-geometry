#pragma once

#include "application/editable_nurbs_surface.hpp"

#include <filesystem>

namespace occt_lab::document
{
[[nodiscard]] auto save_nurbs_surface_project(const std::filesystem::path &path, const EditableNurbsSurface &model)
    -> std::expected<void, std::string>;
[[nodiscard]] auto open_nurbs_surface_project(const std::filesystem::path &path)
    -> std::expected<EditableNurbsSurface, std::string>;
} // namespace occt_lab::document
