#pragma once

#include "application/editable_patch.hpp"

#include <filesystem>

namespace occt_lab::document
{
[[nodiscard]] auto save_patch_project(const std::filesystem::path &path, const EditablePatch &model)
    -> std::expected<void, std::string>;
[[nodiscard]] auto open_patch_project(const std::filesystem::path &path) -> std::expected<EditablePatch, std::string>;
} // namespace occt_lab::document
